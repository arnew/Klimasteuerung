#include <Arduino.h>


#include <Adafruit_SleepyDog.h>



#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

Adafruit_BME280 bme1,bme2; // I2C

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
  if(!bme1.begin(0x76)) {
      Serial.println("Could not find a valid internal BME280 sensor, check wiring!");
  }
  if(!bme2.begin(0x77)) {
      Serial.println("Could not find a valid external BME280 sensor, check wiring!");
  }

  Serial.println("Hello World!");
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

float Tsh = 20, Tsl = 15;

typedef struct {
  unsigned int p: 1, u: 1, w: 1, c: 1, f: 1;

  unsigned long kondensation:32;
  unsigned long trocknen:32;
  unsigned long waermen:32;
  unsigned long kuehlen:32;
  unsigned long fail:32;
  unsigned long duty:32;
  unsigned long zyklen:32;
} debug;
debug d;


bool fan = false;

#define HOUR (60*60/2)
//#define HOUR (10)

unsigned short duty_h[24] =
{ 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0
};
//{0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700, 1800, 1600, 1400, 1200, 1000, 800};

static unsigned char last_hour = 0;
float get_duty(unsigned i) {
  float a = duty_h[i % 24];
  if (i == last_hour) a /= min(d.zyklen, HOUR);
  else a /= HOUR;
  return a;
}
float duty24h() {
  float r = 0;
  for (int i = 0; i < 24; i++) {
    r += get_duty(i);
  }
  return r / min(d.zyklen/HOUR+1, 24);
}

void update_duty(bool fan) {
  d.zyklen++;

  unsigned hour = (d.zyklen / (HOUR)) % 24;
  if (hour != last_hour) {
    duty_h[hour] = 0;
  }
  last_hour = hour;

  duty_h[hour] += fan;
  d.duty += fan;
}

bool update_aussen() {
  float t, h;
  t = bme2.readTemperature();
  h = bme2.readHumidity();
  if (t < -100 || h >= 100) return false;
  Ta = t; rHa = h;
  Tpa = dewPointFast(Ta, rHa);
  Ha = absHum(Ta, rHa);
  return true;
}

bool update_innen() {
  float t, h;
  t = bme1.readTemperature();
  h = bme1.readHumidity();
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
        float dt = get_duty(((24 + last_hour - i) % 24));
        display.fillRect(56 + 3 * i, 31 - 7 * dt , dt ? 2 : 1, dt ? 8 : 1, WHITE);
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
  d.p = d.u = d.w = d.c = 0;
  if (d.p = (Tpa > Ti * 0.8)) goto no_fan;
  // nur trocknen:
  if (d.u = (Ha > Hi * 1.25)) goto no_fan;
  // zu kalt, aber aussen nicht waermer
  if (d.w = (Ti < Tsl && Ta < Ti)) {
    if (Ta < 0 && (duty24h() > 0.01 || get_duty(0) > 0.01)) goto no_fan;
    if (Ta < 5 && (duty24h() > 0.05 || get_duty(0) > 0.05)) goto no_fan;
    if ((duty24h() > 0.10 || get_duty(0) > 0.10)) goto no_fan;
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
  if (update_aussen() & update_innen())  {
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
