/*
  File: main.ino
  Author: Naveen (Embedded Linux Engineer)
  Owner: @spikynavin
  Description:This will control your desktop pc power on, off, reset wirelessly.
  Created: 2025-07-02
*/

#define SINRICPRO_NOSSL
#define ENABLE_DEBUG
#define FIRMWARE_VERSION "1.1.6"

#ifdef ENABLE_DEBUG
  #define DEBUG_ESP_PORT Serial
  #define NODEBUG_WEBSOCKETS
  #define NDEBUG
#endif

#include <Arduino.h>
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include "ESP8266OTAHelper.h"
#endif

#include "SemVer.h"
#include <SinricPro.h>
#include <SinricProSwitch.h>
#include "ArduinoJson.h"

// ======= WiFi Credentials =======
#define WIFI_SSID "*****"
#define WIFI_PASS "*****"

// ======= Static IP Configuration =======
#define BAUD_RATE 115200

#define SET_FIXED_IP_ADDRESS "pro.sinric::set.fixed.ip.address"

// ======= SinricPro Credentials =======
#define APP_KEY "sinric app key"
#define APP_SECRET "sinric app secret key"
#define SWITCH_ID "sinric switch id"

// ======= Config Pin informations =======
const char ON = '+', OFF = '-', SLEEP = '/', AUTO = 'A';
const int pinPowerLightOut = D1;
const int pinPowerLightIn  = D5;
const int pinPowerButton   = D6;
const int pinResetButton   = D7;
const int buttonPressTime  = 250;

char currentStatus = OFF;
char lightStatus = AUTO;

volatile bool statusChangedFlag = false;
unsigned long autoDisconnectTime = 0;
unsigned long powerButtonReleaseTime = 0;
unsigned long lastOn = 0, lastOff = 0;

bool commandReceived = false;

WiFiServer wifiServer(80);
WiFiClient client;

ICACHE_RAM_ATTR void statusChangeISR() {
  statusChangedFlag = true;
}

bool handleOTAUpdate(const String& url, int major, int minor, int patch, bool forceUpdate) {
  Version currentVersion  = Version(FIRMWARE_VERSION);
  Version newVersion      = Version(String(major) + "." + String(minor) + "." + String(patch));
  bool updateAvailable    = newVersion > currentVersion;

  Serial.print("URL: ");
  Serial.println(url.c_str());
  Serial.print("Current version: ");
  Serial.println(currentVersion.toString());
  Serial.print("New version: ");
  Serial.println(newVersion.toString());
  if (forceUpdate) Serial.println("Enforcing OTA update!");

  if (forceUpdate || updateAvailable) {
    if (updateAvailable) {
      Serial.println("Update available!");
    }
    String result = startOtaUpdate(url);
    if (!result.isEmpty()) {
      SinricPro.setResponseMessage(std::move(result));
      return false;
    } 
    return true;
  } else {
    String result = "Current version is up to date.";
    SinricPro.setResponseMessage(std::move(result));
    Serial.println(result);
    return false;
  }
}

bool onSetModuleSetting(const String &id, const String &value) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, value);

  if (error) {
    Serial.print(F("onSetModuleSetting::deserializeJson() failed: "));
    Serial.println(error.f_str());
    return false;
  }

  if (id == SET_FIXED_IP_ADDRESS) {
    String localIP = doc["localIP"];
    String gateway = doc["gateway"];
    String subnet = doc["subnet"];
    String dns1 = doc["dns1"] | "";
    String dns2 = doc["dns2"] | "";

    Serial.printf("localIP:%s, gateway:%s, subnet:%s, dns1:%s, dns2:%s\r\n", localIP.c_str(), gateway.c_str(), subnet.c_str(), dns1.c_str(), dns2.c_str());
    return true;
  } 
  return false;
}

void changeStatusTo(char newStatus) {
  if (newStatus != currentStatus) {
    Serial.print("Status changed to ");
    Serial.println(newStatus);
  }
  currentStatus = newStatus;
}

void recordLightStatusChange(bool lightIn) {
  if (lightIn) lastOn = millis();
  else lastOff = millis();

  if (lightStatus == AUTO)
    digitalWrite(pinPowerLightOut, lightIn);
}

void statusChange() {
  bool currentLightStatus = !digitalRead(pinPowerLightIn);

  if ((lastOn != 0 && lastOff != 0) &&
      (millis() < lastOn + 4000) && (millis() < lastOff + 4000)) {
    changeStatusTo(SLEEP);
  } else {
    changeStatusTo(currentLightStatus ? ON : OFF);
  }

  recordLightStatusChange(currentLightStatus);
}

bool onPowerState(const String &deviceId, bool &state) {
  if (deviceId != SWITCH_ID) return false;

  if (millis() < 10000) {
    Serial.println("Ignored power state change during boot.");
    return true;
  }

  Serial.printf("[SinricPro] Alexa => %s\n", state ? "ON" : "OFF");
  commandReceived = true;
  pressPowerButton(buttonPressTime);
  return true;
}

void pressPowerButton(int duration) {
  if (!commandReceived) {
    Serial.println("Ignored pressPowerButton() - No command received yet.");
    return;
  }

  powerButtonReleaseTime = millis() + duration;
  digitalWrite(pinPowerButton, HIGH);
  Serial.printf("Power button pressed for %d ms\n", duration);
}

void setLightStatus(char newStatus) {
  lightStatus = newStatus;
  if (newStatus == ON) {
    digitalWrite(pinPowerLightOut, HIGH);
    Println("Light On");
  } else if (newStatus == OFF) {
    digitalWrite(pinPowerLightOut, LOW);
    Println("Light Off");
  } else if (newStatus == AUTO) {
    Println("Light Auto");
    digitalWrite(pinPowerLightOut, (currentStatus == ON));
  }
}

void processCommand(String command) {
  commandReceived = true;
  command.trim();
  Serial.print("Received command: ");
  Serial.println(command);

  if (command.equalsIgnoreCase("status")) {
    Print("Desktop Power Status: ");
    if (currentStatus == ON) Println("On");
    else if (currentStatus == OFF) Println("Off");
    else if (currentStatus == SLEEP) Println("Sleeping");
  } else if (command.equalsIgnoreCase("on")) {
    if (currentStatus != ON) {
      Println("Desktop Power Button Pressed (powering on)");
      pressPowerButton(buttonPressTime);
    } else Println("Desktop Computer Already on");
  } else if (command.equalsIgnoreCase("off")) {
    if (currentStatus == ON) {
      Println("Desktop Power Button Pressed (powering off)");
      pressPowerButton(buttonPressTime);
    } else Println("Desktop Computer Already off");
  } else if (command.equalsIgnoreCase("reset")) {
    digitalWrite(pinResetButton, HIGH);
    delay(500);
    digitalWrite(pinResetButton, LOW);
    Println("Desktop Reset Button Pressed");
  } else if (command.equalsIgnoreCase("Desktop force off")) {
    if (currentStatus != OFF) {
      Println("Holding Power Button for 10 seconds...");
      pressPowerButton(10000);
    }
  } else if (command.equalsIgnoreCase("light on")) setLightStatus(ON);
  else if (command.equalsIgnoreCase("light off")) setLightStatus(OFF);
  else if (command.equalsIgnoreCase("light auto")) setLightStatus(AUTO);
}

void Print(String msg) {
  if (client.connected()) {
    client.print(msg);
    client.flush();
  }
  Serial.print("Sent: ");
  Serial.print(msg);
}

void Println(String msg) {
  if (client.connected()) {
    client.println(msg);
    client.flush();
  }
  Serial.println("Sent: " + msg);
}

void checkIfPowerNeedsToRelease() {
  if (powerButtonReleaseTime && millis() >= powerButtonReleaseTime) {
    digitalWrite(pinPowerButton, LOW);
    powerButtonReleaseTime = 0;
    Println("Power Button Released");
  }
}

void runRoutineChecks() {
  checkIfPowerNeedsToRelease();

  if (statusChangedFlag) {
    statusChangedFlag = false;
    statusChange();
  }

  if ((millis() > lastOn + 4000) && (millis() > lastOff + 4000)) {
    statusChange();
  }

  yield();
}

void setupSinricPro() {
  SinricProSwitch& mySwitch = SinricPro[SWITCH_ID];

  SinricPro.onConnected([]() {
    Serial.printf("Connected to SinricPro\r\n");
  });

  SinricPro.onDisconnected([]() {
    Serial.printf("Disconnected from SinricPro\r\n");
    commandReceived = false;
  });

  SinricPro.onOTAUpdate(handleOTAUpdate);
  SinricPro.onSetSetting(onSetModuleSetting);
  SinricPro.begin(APP_KEY, APP_SECRET);
  SinricPro.restoreDeviceStates(false);
}

void setup() {
  Serial.begin(BAUD_RATE);

  pinMode(pinPowerLightIn, INPUT_PULLUP);
  pinMode(pinPowerLightOut, OUTPUT);
  pinMode(pinPowerButton, OUTPUT);
  digitalWrite(pinPowerButton, LOW);
  pinMode(pinResetButton, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(pinPowerLightIn), statusChangeISR, CHANGE);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.printf(".");
    delay(250);
  }
  Serial.printf("connected!\r\n[WiFi]: IP-Address is %s\r\n", WiFi.localIP().toString().c_str());
  
  wifiServer.begin();

  setupSinricPro();
  SinricProSwitch& mySwitch = SinricPro[SWITCH_ID];
  mySwitch.onPowerState(onPowerState);
}

void loop() {
  runRoutineChecks();

  client = wifiServer.available();
  if (client) {
    Serial.println("Client connected");
    String comm = "";
    unsigned long timeout = millis() + 60000;

    while (client.connected() && millis() < timeout) {
      while (client.available()) {
        char c = client.read();
        if (c == '\r' || c == '\n') continue;

        if (c == ';') {
          processCommand(comm);
          comm = "";
        } else {
          comm += c;
        }

        timeout = millis() + 60000;
      }
      runRoutineChecks();
    }

    client.stop();
    Serial.println("Client disconnected");
  }

  SinricPro.handle();
}
