/* Node03 responsibilities:
   - Tweet stats each night around midnight. Blynk data pulled in using Blynk.syncVirtual.
   - Reports LK's bedroom temperature and alarm based on app-set high temp setpoint.
*/

#include <SimpleTimer.h>
#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

#include <TimeLib.h>            // Used by WidgetRTC.h
#include <WidgetRTC.h>          // Blynk's RTC

#include <ESP8266mDNS.h>        // Required for OTA
#include <WiFiUdp.h>            // Required for OTA
#include <ArduinoOTA.h>         // Required for OTA

float tempLK;                   // Room temp
int tempLKhighAlarm = 200;
bool isFirstConnect = true;

int yMonth, yDate, yYear;
bool tweetStartedFlag;
int getWait = 3000;             // Duration to wait between vPin syncs from Blynk
int tempAtticHigh, tempHouseHigh, tempHouseLow, tempOutsideHigh, tempOutsideLow;
String runtimeTotal;

char auth[] = "fromBlynkApp";

const char* ssid = "ssid";
const char* pw = "pw";

SimpleTimer timer;
WidgetTerminal terminal(V26);
WidgetRTC rtc;
BLYNK_ATTACH_WIDGET(rtc, V8);

void setup()
{
  Serial.begin(9600);
  Blynk.begin(auth, ssid, pw);

  WiFi.softAPdisconnect(true); // Per https://github.com/esp8266/Arduino/issues/676 this turns off AP

  while (Blynk.connect() == false) {
    // Wait until connected
  }

  sensors.begin();
  sensors.setResolution(10);

  // START OTA ROUTINE
  ArduinoOTA.setHostname("esp8266-Node03LK");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC address: ");
  Serial.println(WiFi.macAddress());
  // END OTA ROUTINE

  rtc.begin();

  timer.setInterval(2000L, sendTemps);    // Temperature sensor polling interval
  timer.setInterval(1000L, uptimeReport);
}

void loop()
{
  Blynk.run();
  timer.run();
  ArduinoOTA.handle();

  if (hour() == 23 && minute() == 59 && second() >= 0 && tweetStartedFlag == 0)
  {
    yMonth = month();
    yDate = day();
    yYear = year();
    timer.setTimeout(1500L, tweetSync1);  // Kicks off the process ending with a Tweet just around midnight.
    tweetStartedFlag = 1;                 // Makes sure this process starts only once at 11:59pm.
  }
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

BLYNK_WRITE(V32) // Force Tweet for debugging
{
  int pinData = param.asInt();

  if (pinData == 0)
  {
    yMonth = month();
    yDate = day();
    yYear = year();
    timer.setTimeout(1500L, tweetSync1);  // Kicks off the process ending with a Tweet just around midnight.
    tweetStartedFlag = 1;                 // Makes sure this process starts only once at 11:59pm.
  }
}

void uptimeSend()  // Blinks a virtual LED in the Blynk app to show the ESP is live and reporting.
{
  long minDur = millis() / 60000L;
  long hourDur = millis() / 3600000L;
  if (minDur < 121)
  {
    terminal.print(String("Node03 (LK): ") + minDur + " mins @ ");
    terminal.println(WiFi.localIP());
  }
  else if (minDur > 120)
  {
    terminal.print(String("Node03 (LK): ") + hourDur + " hrs @ ");
    terminal.println(WiFi.localIP());
  }

  terminal.flush();
}

void uptimeReport() {
  if (second() > 3 && second() < 8)
  {
    Blynk.virtualWrite(103, minute());
  }
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

// START INFO GATHERING FOR TWEET

void tweetSync1() {
  Blynk.syncVirtual(V24);
  timer.setTimeout(getWait, tweetSync2);
}

BLYNK_WRITE(V24) {
  tempAtticHigh = param.asInt();
}


void tweetSync2() {
  Blynk.syncVirtual(V22);
  timer.setTimeout(getWait, tweetSync3);
}

BLYNK_WRITE(V22) {
  tempHouseHigh = param.asInt();
}


void tweetSync3() {
  Blynk.syncVirtual(V23);
  timer.setTimeout(getWait, tweetSync4);
}

BLYNK_WRITE(V23) {
  tempHouseLow = param.asInt();
}


void tweetSync4() {
  Blynk.syncVirtual(V5);
  timer.setTimeout(getWait, tweetSync5);
}

BLYNK_WRITE(V5) {
  tempOutsideHigh = param.asInt();
}


void tweetSync5() {
  Blynk.syncVirtual(V13);
  timer.setTimeout(getWait, tweetSync6);
}

BLYNK_WRITE(V13) {
  tempOutsideLow = param.asInt();
}


void tweetSync6() {
  Blynk.syncVirtual(V15);
  timer.setTimeout(2000, dailyTweet);
}

BLYNK_WRITE(V15) {
  runtimeTotal = param.asString();
}


void dailyTweet()
{
  if (runtimeTotal.length() > 10) {     // Differentiates between "None" and anything else reporting runtime and run qty.
    Blynk.tweet(String("On ") + yMonth + "/" + yDate + "/" + yYear + ", House: " + tempHouseHigh + "/" + tempHouseLow + ", Outside: " + tempOutsideHigh + "/" + tempOutsideLow + ", Attic High: " + tempAtticHigh + ", and HVAC ran for " + runtimeTotal + ".");
  }
  else {
    Blynk.tweet(String("On ") + yMonth + "/" + yDate + "/" + yYear + ", House: " + tempHouseHigh + "/" + tempHouseLow + ", Outside: " + tempOutsideHigh + "/" + tempOutsideLow + ", Attic High: " + tempAtticHigh + ", and HVAC did not run.");
  }

  tweetStartedFlag = 0;         // Ready for the next tweet (at the next 11:59pm).
  Serial.println("Tweet!");
}
