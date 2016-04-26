#include <SimpleTimer.h>
#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <TimeLib.h>  // Remove?
#define ONE_WIRE_BUS 2

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

DeviceAddress ds18b20lk = { 0x28, 0xEE, 0x27, 0x16, 0x01, 0x16, 0x02, 0xED }; // LK

char auth[] = "fromBlynkApp *04";

SimpleTimer timer;

WidgetLED led1(V9);

void setup()
{
  Serial.begin(9600);
  Blynk.begin(auth, "ssid", "pw");

  WiFi.softAPdisconnect(true); // Per https://github.com/esp8266/Arduino/issues/676 this turns off AP

  sensors.begin();
  sensors.setResolution(ds18b20lk, 10);

  timer.setInterval(2000L, sendTemps); // Temperature sensor polling interval
  timer.setInterval(5000L, sendHeartbeat);
}

void sendTemps()
{
  sensors.requestTemperatures(); // Polls the sensors

  float tempLK = sensors.getTempF(ds18b20lk);

  if (tempLK > 0)
  {
    Blynk.virtualWrite(6, tempLK);
  }
  else
  {
    Blynk.virtualWrite(6, "ERR");
  }
    led1.off();
}

void sendHeartbeat()
{
  led1.on();
}

void loop()
{
  Blynk.run();
  timer.run();
}
