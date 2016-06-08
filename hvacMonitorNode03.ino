#include <SimpleTimer.h>
#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

float tempLK; // Room temp
int tempLKhighAlarm = 200;
bool isFirstConnect = true;
char auth[] = "fromBlynkApp";

SimpleTimer timer;
WidgetLED led1(V9); // Heartbeat LED
WidgetTerminal terminal(V26);

void setup()
{
  Serial.begin(9600);
  Blynk.begin(auth, "ssid", "pw");

  WiFi.softAPdisconnect(true); // Per https://github.com/esp8266/Arduino/issues/676 this turns off AP

  while (Blynk.connect() == false) {
    // Wait until connected
  }

  sensors.begin();
  sensors.setResolution(10);

  timer.setInterval(2000L, sendTemps); // Temperature sensor polling interval

  heartbeatOn();
}

void loop()
{
  Blynk.run();
  timer.run();
}

BLYNK_CONNECTED() {
  if (isFirstConnect) {
    Blynk.syncAll();
  }
}

BLYNK_WRITE(V27) // App button to report uptime
{
  int pinData = param.asInt();

  if (pinData == 0)
  {
    timer.setTimeout(9000L, uptimeSend);
  }
}

void uptimeSend()  // Blinks a virtual LED in the Blynk app to show the ESP is live and reporting.
{
  long minDur = millis() / 60000L;
  long hourDur = millis() / 3600000L;
  if (minDur < 121)
  {
    terminal.println(String("Node03 (LK): ") + minDur + " mins ");
  }
  else if (minDur > 120)
  {
    terminal.println(String("Node03 (LK): ") + hourDur + " hours ");
  }

  terminal.flush();
}

void heartbeatOn()  // Blinks a virtual LED in the Blynk app to show the ESP is live and reporting.
{
  led1.on();
  timer.setTimeout(2500L, heartbeatOff);
}

void heartbeatOff()
{
  led1.off();  // The OFF portion of the LED heartbeat indicator in the Blynk app
  timer.setTimeout(2500L, heartbeatOn);
}

// Input from Blynk app menu to select room temperature that triggers alarm
BLYNK_WRITE(V21) {
  switch (param.asInt())
  {
    case 1: { // Alarm Off
        tempLKhighAlarm = 200;
        //Serial.println("Case 1 set: Alarm off");
        break;
      }
    case 2: { // 80F Alarm
        tempLKhighAlarm = 80;
        break;
      }
    case 3: { // 81F Alarm
        tempLKhighAlarm = 81;
        break;
      }
    case 4: { // 82F Alarm;
        tempLKhighAlarm = 82;
        //Serial.println("Case 4 set: 82F");
        break;
      }
    case 5: { // 83F Alarm
        tempLKhighAlarm = 83;
        break;
      }
    case 6: { // 84F Alarm
        tempLKhighAlarm = 84;
        break;
      }
    default: {
        Serial.println("Unknown item selected");
      }
  }
}

void notifyAndOff()
{
  Blynk.notify(String("Liv's room is ") + tempLK + "°F. Alarm disabled until reset."); // Send notification.
}

void sendTemps()
{
  sensors.requestTemperatures(); // Polls the sensors

  tempLK = sensors.getTempFByIndex(0);

  if (tempLK > 0)
  {
    Blynk.virtualWrite(6, tempLK);
  }
  else
  {
    Blynk.virtualWrite(6, "ERR");
  }

  if (tempLK >= tempLKhighAlarm)
  {
    notifyAndOff();
    Blynk.virtualWrite(V21, 1); // Rather than fancy timing, just disable alarm until reset.
    tempLKhighAlarm = 200;
  }
}
