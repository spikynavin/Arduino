#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <WiFiServer.h>

// Replace with your actual WiFi credentials
const char* ssid = "Your-wifi-ssid";
const char* password = "Your-wifi-password";

// ⚙️ Static IP config — adjust for your network
IPAddress local_IP(192, 168, 0, 200);       // Desired static IP
IPAddress gateway(192, 168, 0, 1);          // Usually your router IP
IPAddress subnet(255, 255, 255, 0);         // Default subnet mask
IPAddress primaryDNS(8, 8, 8, 8);           // Optional
IPAddress secondaryDNS(8, 8, 4, 4);         // Optional

// System status constants
const char ON = '+', OFF = '-', SLEEP = '/', AUTO = 'A';

// GPIO Pins
const int pinPowerLightOut = D1;
const int pinPowerLightIn = D5;
const int pinPowerButton = D6;
const int pinResetButton = D7;

const int buttonPressTime = 250; // Duration for short power button press

char currentStatus = OFF;
char lightStatus = AUTO;

volatile bool statusChangedFlag = false; // Used by ISR

unsigned long autoDisconnectTime = 0;
unsigned long powerButtonReleaseTime = 0;
unsigned long lastOn = 0;
unsigned long lastOff = 0;

WiFiServer wifiServer(80);
WiFiClient client;

// Mark ISR as IRAM-safe
ICACHE_RAM_ATTR void statusChangeISR() {
  statusChangedFlag = true;
}

void changeStatusTo(char newStatus) {
  if (newStatus != currentStatus) {
    Serial.print("Status changed to ");
    Serial.println(newStatus);
  }
  currentStatus = newStatus;
}

void recordLightStatusChange(bool lightIn) {
  if (lightIn)
    lastOn = millis();
  else
    lastOff = millis();

  if (lightStatus == AUTO)
    digitalWrite(pinPowerLightOut, lightIn);
}

void statusChange() {
  bool currentLightStatus = !digitalRead(pinPowerLightIn);

  if (lastOn != 0 && lastOff != 0) {
    if ((millis() < lastOn + 4000) && (millis() < lastOff + 4000)) {
      changeStatusTo(SLEEP);
      recordLightStatusChange(currentLightStatus);
      return;
    }
  }

  if (currentLightStatus)
    changeStatusTo(ON);
  else
    changeStatusTo(OFF);

  recordLightStatusChange(currentLightStatus);
}

void Print(String toPrint) {
  if (client.connected()) {
    client.print(toPrint);
    client.flush(); // Ensure it's sent
  }
  Serial.print("Sent to client: ");
  Serial.print(toPrint);
}

void Println(String toPrint) {
  if (client.connected()) {
    client.println(toPrint);
    client.flush(); // Ensure it's sent
  }
  Serial.print("Sent to client: ");
  Serial.println(toPrint);
}

void processCommand(String command) {
  Serial.print(F("Received command: "));
  Serial.println(command);

  if (command.equalsIgnoreCase("status")) {
    Print("Status: ");
    if (currentStatus == ON) Println("On");
    else if (currentStatus == OFF) Println("Off");
    else if (currentStatus == SLEEP) Println("Sleeping");
  }
  else if (command.equalsIgnoreCase("on")) {
    if (currentStatus != ON) {
      Println("Power Button Pressed (powering on)");
      pressPowerButton(buttonPressTime);
    } else Println("Machine is already on (button not pressed)");
  }
  else if (command.equalsIgnoreCase("off")) {
    if (currentStatus == ON) {
      Println("Power Button Pressed (powering off)");
      pressPowerButton(buttonPressTime);
    } else Println("Machine is already off (button not pressed)");
  }
  else if (command.equalsIgnoreCase("reset")) {
    digitalWrite(pinResetButton, HIGH);
    delay(500); // Safer than tight loop
    digitalWrite(pinResetButton, LOW);
    Println("Reset Button Pressed");
  }
  else if (command.equalsIgnoreCase("force off")) {
    if (currentStatus != OFF) {
      Println("Holding Power Button for 10 seconds...");
      pressPowerButton(10000);
    }
  }
  else if (command.equalsIgnoreCase("pressPwr")) {
    digitalWrite(pinPowerButton, HIGH);
    Println("Power Button Pressed");
  }
  else if (command.equalsIgnoreCase("releasePwr")) {
    digitalWrite(pinPowerButton, LOW);
    Println("Power Button Released");
  }
  else if (command.equalsIgnoreCase("light on")) {
    setLightStatus(ON);
  }
  else if (command.equalsIgnoreCase("light off")) {
    setLightStatus(OFF);
  }
  else if (command.equalsIgnoreCase("light auto")) {
    setLightStatus(AUTO);
  }
}

void setLightStatus(char newStatus) {
  lightStatus = newStatus;

  if (newStatus == ON) {
    digitalWrite(pinPowerLightOut, HIGH);
    Println("Light On");
  }
  else if (newStatus == OFF) {
    digitalWrite(pinPowerLightOut, LOW);
    Println("Light Off");
  }
  else if (newStatus == AUTO) {
    Println("Light Set to Auto");
    // Update light based on current status
    digitalWrite(pinPowerLightOut, (currentStatus == ON));
  }
}

void pressPowerButton(int duration) {
  powerButtonReleaseTime = millis() + duration;
  digitalWrite(pinPowerButton, HIGH);
}

void runRoutineChecks() {
  checkIfPowerNeedsToRelease();

  if (statusChangedFlag) {
    statusChangedFlag = false;
    statusChange();  // Safe now, outside ISR
  }

  if ((millis() > lastOn + 4000) && (millis() > lastOff + 4000)) {
    statusChange();
  }
}

void checkIfPowerNeedsToRelease() {
  if (powerButtonReleaseTime != 0 && millis() >= powerButtonReleaseTime) {
    digitalWrite(pinPowerButton, LOW);
    powerButtonReleaseTime = 0;
    Println("Power Button Released");
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(pinPowerLightIn, INPUT_PULLUP);
  pinMode(pinPowerLightOut, OUTPUT);
  pinMode(pinPowerButton, OUTPUT);
  pinMode(pinResetButton, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(pinPowerLightIn), statusChangeISR, CHANGE);

  // 🧷 Set static IP
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("Failed to configure static IP");
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.print("Connected to WiFi. IP: ");
  Serial.println(WiFi.localIP());

  wifiServer.begin();
}

void loop() {
  runRoutineChecks();

  client = wifiServer.available();
  if (client) {
    Serial.println("Client connected");

    String comm = "";
    unsigned long timeout = millis() + 60000;

    while (client.connected() && millis() < timeout) {
      while (client.available() > 0) {
        char c = client.read();

        // Ignore carriage return and newline characters
        if (c == '\r' || c == '\n') {
          continue;
        }

        Serial.print("Received char: ");
        Serial.println(c);

        if (c == ';') {
          Serial.print("Full command: ");
          Serial.println(comm);

          client.print("You sent: ");
          client.println(comm);

          processCommand(comm);

          client.flush();  // Ensure all data is sent
          delay(10);       // Small delay for client compatibility

          comm = ""; // Clear command buffer
        } else {
          comm += c;
        }

        timeout = millis() + 60000;  // Reset timeout on every char received
      }

      runRoutineChecks();
    }

    client.stop();
    Serial.println("Client disconnected");
  }

}
