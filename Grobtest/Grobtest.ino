

#include <Adafruit_SleepyDog.h>

#include <SimpleDHT.h>

// for DHT22,
//      VCC: 5V or 3V
//      GND: GND
//      DATA: 2
int pinDHT22 = 2;
SimpleDHT22 dht22(pinDHT22);


// for DHT11,
//      VCC: 5V or 3V
//      GND: GND
//      DATA: 3
int pinDHT11 = 3;
SimpleDHT11 dht11(pinDHT11);

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

Adafruit_BME280 bme; // I2C

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(128, 32, &Wire, -1);

//------------------------------------------------------------------------------

void setup() {
  int countdownMS = Watchdog.enable(4000);


  Serial.begin(115200);
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
  }

  // default settings
  // (you can also pass in a Wire library object like &Wire2)
  bool status = bme.begin();
  if (!status) {
    status = bme.begin(0x76);
    if (!status) {
      Serial.println("Could not find a valid BME280 sensor, check wiring!");
    }
  }

  pinMode(13, OUTPUT);
}
//------------------------------------------------------------------------------

double dewPointFast(double celsius, double humidity)
{
  double a = 17.271;
  double b = 237.7;
  double temp = (a * celsius) / (b + celsius) + log(humidity * 0.01);
  double Td = (b * temp) / (a - temp);
  return Td;
}

double absHum(double celsius, double humidity)
{
  return 6.112 * pow(2.71828, (17.67 * celsius) / (celsius + 243.5)) * humidity * 18.02 / ( (273.15 + celsius) * 100 * 0.08314);
}

float Ta, rHa, Tpa, Hi;

float Ti, rHi, Tpi, Ha;

float Tsh = 20, Tsl = 10;

typedef struct {
  unsigned int p: 1, u: 1, w: 1, c: 1, f: 1;

  unsigned int kondensation;
  unsigned int trocknen;
  unsigned int waermen;
  unsigned int kuehlen;
  unsigned int fail;
  unsigned int duty;
  unsigned int zyklen;
} debug;
debug d;


bool fan = false;


unsigned short duty_h[24] =
{0};
//{0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700, 1800, 1600, 1400, 1200, 1000, 800};

static byte last_hour = 0;
float get_duty(byte i) {
  float a = duty_h[i];
  if (i == last_hour) a /= min(d.zyklen, 60 * 60 / 2);
  else a /= 60 * 60 / 2;
  return a;
}

float duty24h() {
  float r = 0;
  for (int i = 0; i < 24; i++) {
    r += get_duty(i);
  }
  return r / 24;
}

void update_duty(bool fan) {
  d.zyklen++;

  byte hour = (d.zyklen / (60 * 60 / 2)) % 24;
  if (hour != last_hour) {
    duty_h[hour] = 0;
  }
  last_hour = hour;

  duty_h[hour] += fan;
  d.duty += fan;
}

bool update_aussen() {
  int err ;
  if ((err = dht22.read2(&Ta, &rHa, NULL)) != SimpleDHTErrSuccess) {
    return false;
  }
  Tpa = dewPointFast(Ta, rHa);
  Ha = absHum(Ta, rHa);
  return true;
}

bool update_innen() {
  byte t, h;
  int err ;
  if ((err = dht11.read(&t, &h, NULL)) != SimpleDHTErrSuccess) {
    return false;
  }
  Ti = t; rHi = h;
  Tpi = dewPointFast(Ti, rHi);
  Hi = absHum(Ti, rHi);
  return true;
}

bool update_innen_bme() {
  float t, h;
  t = bme.readTemperature();
  h = bme.readHumidity();
  if (t < -100 || h >= 100) return false;
  Ti = t; rHi = h;
  Tpi = dewPointFast(Ti, rHi);
  Hi = absHum(Ti, rHi);
  return true;
}

void show_info() {
  display.clearDisplay();

  display.setTextSize(1);             // Normal 1:1 pixel scale
  display.setTextColor(WHITE);        // Draw white text
  display.setCursor(0, 0);            // Start at top-left corner
  display.print("I ");
  display.print((float) Ti, 1);
  display.print(" ");
  display.print((float) rHi, 0);
  display.print(" ");
  display.print((float) Hi, 1);
  display.print(" ");
  display.print((float) Tpi, 1);
  display.println("");

  display.print("A ");
  display.print((float) Ta, 1);
  display.print(" ");
  display.print((float) rHa, 0);
  display.print(" ");
  display.print((float) Ha, 1);
  display.print(" ");
  display.print((float) Tpa, 1);
  display.println("");

  switch ((d.zyklen / 3) % 3) {
    case 0:

      display.setTextSize(2);
      display.print(fan ? "On  " : "Off ");

      display.setTextSize(1);
      display.print(d.p ? "C" : "");
      display.print(d.u ? "D" : "");
      display.print(d.w ? "L" : "");
      display.print(d.c ? "H" : "");
      display.print(" ");
      display.print(d.f ? "F" : "");
      display.setCursor(40, 24);
      display.print(d.zyklen);
      break;
    case 1:

      display.setTextSize(2);
      display.print((float)(100.0 * d.duty / d.zyklen), 0);
      display.print("%");
      display.setTextSize(1);
      display.print("h:");
      display.print((float)100 * get_duty(0), 0);
      display.print(" d:");
      display.print((float)100 * duty24h(), 0);

      // 56 - 128 24-32
      for (int i = 0; i < 24; i++)  {
        byte hour = (d.zyklen / (60 * 60 / 2)) % 24;
        float dt = get_duty((last_hour + i) % 24);
        display.fillRect(56 + 3 * i, 31 - 7 * dt , 2, 8, WHITE);
      }

      break;
    case 2:
      display.setTextSize(1);
      display.print("C "); display.print((int)(100.0 * d.kondensation / d.zyklen));
      display.print(" D "); display.print((int)(100.0 * d.trocknen / d.zyklen));
      display.print(" F "); display.print((int)(100.0 * d.fail / d.zyklen));
      display.println("");

      display.print("L "); display.print((int)(100.0 * d.waermen / d.zyklen));
      display.print(" H "); display.print((int)(100.0 * d.kuehlen / d.zyklen));

      display.println("");
      break;
  }


  display.display();


  Serial.print((float) Ti);
  Serial.print(" ");
  Serial.print((float) rHi);
  Serial.print(" ");
  Serial.print((float) Hi);
  Serial.print(" ");
  Serial.print((float) Tpi);
  Serial.print(" ");
  Serial.print((float) Ta);
  Serial.print(" ");
  Serial.print((float) rHa);
  Serial.print(" ");
  Serial.print((float) Ha);
  Serial.print(" ");
  Serial.print((float) Tpa);
  Serial.print(" "); Serial.print((float)fan * 100);
  Serial.print(" "); Serial.print((float)(100.0 * d.kondensation / d.zyklen));
  Serial.print(" "); Serial.print((float)(100.0 * d.trocknen / d.zyklen));
  Serial.print(" "); Serial.print((float)(100.0 * d.waermen / d.zyklen));
  Serial.print(" "); Serial.print((float)(100.0 * d.kuehlen / d.zyklen));
  Serial.print(" "); Serial.print((float)(100.0 * d.fail / d.zyklen));
  Serial.print(" "); Serial.print((float)(100.0 * d.duty / d.zyklen));
  Serial.print(" "); Serial.print((float)100.0 * duty24h());

  Serial.print(" "); Serial.print(abs((int)(
                                        d.zyklen % 200) - 100));


  Serial.println("");

}


void decide() {
  // innen nicht kondensieren:
  if (d.p = (Tpa > Ti * 0.8)) goto no_fan;
  // nur trocknen:
  if (d.u = (Ha > Hi * 1.25)) goto no_fan;
  // zu kalt, aber aussen nicht waermer
  if (d.w = (Ti < Tsl && Ta < Ti)) {
    if (Ti < 0 && (duty24h() > 0.05 || get_duty(0) > 0.5)) goto no_fan;
    if (Ti < 5 && (duty24h() > 0.15 || get_duty(0) > 0.5)) goto no_fan;
    if ((duty24h() > 0.3 || get_duty(0) > 0.5)) goto no_fan;
  }
  // zu warm, aber aussen nicht kaelter
  if (d.c = (Ti > Tsh && Ta > Ti)) goto no_fan;

fan_on:
  fan = true;
  return;
no_fan:
  fan = false;
  return;
}

void update_fan() {

  digitalWrite(13, fan ? HIGH : LOW);
}

void force_fan(byte seconds) {

  digitalWrite(13, HIGH);
  delay(seconds * 1000);
  update_fan();
}

void loop() {
  Watchdog.reset();
  //if(!fan) {
  //force_fan(10);
  //}
  if (update_aussen() && (update_innen() || update_innen_bme()))  {
    decide();
    d.kondensation += d.p;
    d.trocknen += d.u;
    d.waermen += d.w;
    d.kuehlen += d.c;
    d.f = 0;
  } else {
    fan = false;
    d.f = 1;
    d.fail += 1;
    if (d.fail > 0.1 * d.zyklen) {
      Serial.println("more than 10% read errors - resetting");
      for (;;);
    }
  }

  update_duty(fan);

  show_info();

  update_fan();
  //if(fan) {
  //  // DHT22 sampling rate is 0.5HZ.
  //  delay(2500);
  //} else {
  //  oled.println("sleeping");
  //  delay(10000);
  //}
  delay(2000);


}
