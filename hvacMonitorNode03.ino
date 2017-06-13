/* Node03 responsibilities:
   - Tweet stats each night around midnight. Blynk data pulled in using Blynk.syncVirtual.
   - Reports LK's bedroom temperature and alarm based on app-set high temp setpoint.
*/

#include <SimpleTimer.h>
#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space
#include <BlynkSimpleEsp8266.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include <TimeLib.h>            // Used by WidgetRTC.h
#include <WidgetRTC.h>          // Blynk's RTC

#include <ESP8266mDNS.h>        // Required for OTA
#include <WiFiUdp.h>            // Required for OTA
#include <ArduinoOTA.h>         // Required for OTA

#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

char auth[] = "fromBlynkApp";
char ssid[] = "ssid";
char pass[] = "pw";

SimpleTimer timer;

WidgetTerminal terminal(V26);
WidgetRTC rtc;
WidgetBridge bridge1(V50);

double tempLK;                // Room temp
int tempLKint;                // Room temp converted to int
int tempLKhighAlarm = 200;    // Set high temp alarm very high as setpoint
bool isFirstConnect = true;

int yMonth, yDate, yYear;
bool tweetStartedFlag;
int getWait = 3000;           // Duration to wait between vPin syncs from Blynk
int tempAtticHigh, tempHouseHigh, tempHouseLow, tempOutsideHigh, tempOutsideLow;
String runtimeTotal;

int dailyHigh = 0;
int dailyLow = 200;

int last24high, last24low;    // Rolling high/low temps in last 24-hours.
int last24hoursTemps[288];    // Last 24-hours temps recorded every 5 minutes.
int arrayIndex = 0;

bool heatingMode;             // FALSE = Cooling. TRUE = Heating. Pulled from hvacMonitor for tweets.

void setup()
{
  Serial.begin(9600);
  Blynk.begin(auth, ssid, pass);

  //WiFi.softAPdisconnect(true); // Per https://github.com/esp8266/Arduino/issues/676 this turns off AP

  while (Blynk.connect() == false) {
    // Wait until connected
  }

  // START OTA ROUTINE
  ArduinoOTA.setHostname("Node03LK-ESP01");

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

  sensors.begin();
  sensors.setResolution(10);

  timer.setTimeout(1000, vsync1);                 // Syncs back vPins to survive hardware reset.
  timer.setInterval(2000L, sendTemps);            // Temperature sensor polling interval
  timer.setInterval(1000L, uptimeReport);         // Records current minute
  timer.setTimeout(5000, setupArray);             // Sets entire array to temp at startup for a "baseline"
  timer.setInterval(30000, sendControlTemp);      // Sends temp to hvacMonitor via bridge for control
  timer.setInterval(300000L, recordHighLowTemps);  // Array updated ~5 minutes
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

  if (hour() == 00 && minute() == 01)
  {
    timer.setTimeout(61000, resetHiLoTemps);
  }

}

void vsync1()
{
  Blynk.syncVirtual(V21);    // tempLKhighAlarm
}

BLYNK_WRITE(V27) // App button to report uptime
{
  int pinData = param.asInt();

  if (pinData == 0)
  {
    timer.setTimeout(9000L, uptimeSend);
  }
}

void sendControlTemp() {
  bridge1.virtualWrite(V127, 3, tempLK);    // Writing "3" for this node, then the temp.
}

BLYNK_CONNECTED() {
  bridge1.setAuthToken(auth); // Place the AuthToken of the second hardware here
}

/*
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
*/

void uptimeSend()
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

void uptimeReport()
{
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
  Blynk.notify(String("Liv's room is ") + tempLK + "F. Alarm disabled until reset."); // Send notification.
  Blynk.virtualWrite(V21, 1);    // Disable alarm until reset.
  Blynk.syncVirtual(V21);
}

void sendTemps()
{
  sensors.requestTemperatures(); // Polls the sensors

  tempLK = sensors.getTempFByIndex(0);

  // Conversion of tempLK to tempLKint
  int xLKint = (int) tempLK;
  double xLK10ths = (tempLK - xLKint);
  if (xLK10ths >= .50)
  {
    tempLKint = (xLKint + 1);
  }
  else
  {
    tempLKint = xLKint;
  }

  // Send temperature to the app display
  if (tempLK > 0)
  {
    Blynk.virtualWrite(6, tempLK);
  }
  else
  {
    Blynk.virtualWrite(6, "ERR");
  }

  // Set the app display color based on temperature
  if (tempLK < 78)
  {
    Blynk.setProperty(V6, "color", "#04C0F8"); // Blue
  }
  else if (tempLK >= 78 && tempLK <= 80)
  {
    Blynk.setProperty(V6, "color", "#ED9D00"); // Yellow
  }
  else if (tempLK > 80)
  {
    Blynk.setProperty(V6, "color", "#D3435C"); // Red
  }

  // Check if temperature is high enough to send alarm notification
  if (tempLK >= tempLKhighAlarm)
  {
    notifyAndOff();
    tempLKhighAlarm = 200;
  }
}

// START INFO GATHERING FOR TWEET

void tweetSync1() {
  Blynk.syncVirtual(V24);
  Blynk.syncVirtual(V22);
  Blynk.syncVirtual(V23);
  timer.setTimeout(getWait, tweetSync2);
}

BLYNK_WRITE(V24) {
  tempAtticHigh = param.asInt();
}

BLYNK_WRITE(V22) {
  tempHouseHigh = param.asInt();
}

BLYNK_WRITE(V23) {
  tempHouseLow = param.asInt();
}

void tweetSync2() {
  Blynk.syncVirtual(V5);
  Blynk.syncVirtual(V13);
  Blynk.syncVirtual(V15);
  timer.setTimeout(getWait, tweetSync3);
}

BLYNK_WRITE(V5) {
  tempOutsideHigh = param.asInt();
}

BLYNK_WRITE(V13) {
  tempOutsideLow = param.asInt();
}

BLYNK_WRITE(V15) {
  runtimeTotal = param.asString();
}

void tweetSync3() {
  Blynk.syncVirtual(V38);
  timer.setTimeout(getWait, dailyTweet);
}

BLYNK_WRITE(V38) {
  String blah = param.asStr();

  if (blah == "COOL")
  {
    heatingMode = FALSE;
  }
  else if (blah == "HEAT")
  {
    heatingMode = TRUE;
  }
}

// STOPPED COLLECTING INFO FOR TWEET. Below sends tweet.

void dailyTweet()
{
  if (runtimeTotal.length() > 10 && heatingMode == FALSE) {     // Differentiates between "None" and anything else reporting runtime and run qty.
    Blynk.tweet(String("On ") + yMonth + "/" + yDate + "/" + yYear + ", House: " + tempHouseHigh + "/" + tempHouseLow + ", Outside: " + tempOutsideHigh + "/" + tempOutsideLow + ", Attic High: " + tempAtticHigh + ", and HVAC cooled for " + runtimeTotal + ".");
  }
  else if (runtimeTotal.length() > 10 && heatingMode == TRUE) {
    Blynk.tweet(String("On ") + yMonth + "/" + yDate + "/" + yYear + ", House: " + tempHouseHigh + "/" + tempHouseLow + ", Outside: " + tempOutsideHigh + "/" + tempOutsideLow + ", Attic High: " + tempAtticHigh + ", and HVAC heated for " + runtimeTotal + ".");
  }
  else {
    Blynk.tweet(String("On ") + yMonth + "/" + yDate + "/" + yYear + ", House: " + tempHouseHigh + "/" + tempHouseLow + ", Outside: " + tempOutsideHigh + "/" + tempOutsideLow + ", Attic High: " + tempAtticHigh + ", and HVAC did not run.");
  }

  tweetStartedFlag = 0;         // Ready for the next tweet (at the next 11:59pm).
  Serial.println("Tweet!");
}

void setupArray()
{
  for (int i = 0; i < 288; i++)
  {
    last24hoursTemps[i] = tempLKint;
  }

  last24high = tempLKint;
  last24low = tempLKint;

  Blynk.setProperty(V6, "label", "Liv");
}

void recordHighLowTemps()
{
  if (arrayIndex < 288)                   // Mess with array size and timing to taste!
  {
    last24hoursTemps[arrayIndex] = tempLKint;
    ++arrayIndex;
  }
  else
  {
    arrayIndex = 0;
    last24hoursTemps[arrayIndex] = tempLKint;
    ++arrayIndex;
  }

  last24high = -200;
  last24low = 200;

  for (int i = 0; i < 288; i++)
  {
    if (last24hoursTemps[i] > last24high)
    {
      last24high = last24hoursTemps[i];
    }

    if (last24hoursTemps[i] < last24low)
    {
      last24low = last24hoursTemps[i];
    }
  }

  if (tempLKint > dailyHigh)
  {
    dailyHigh = tempLKint;
  }

  if (tempLKint < dailyLow)
  {
    dailyLow = tempLKint;
  }

  Blynk.setProperty(V6, "label", String("Liv ") + last24high + "/" + last24low);  // Sets label with high/low temps.
}

void resetHiLoTemps()
{
  dailyHigh = 0;     // Resets daily high temp
  dailyLow = 200;    // Resets daily low temp
}
