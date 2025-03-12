#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Wire.h>
#include <time.h>
#include <WiFiManager.h>

#define SWITCH_PIN 26

WiFiManager wifiManager;

Preferences preferences;
LiquidCrystal_I2C LCD(0x27, 16, 2);

volatile uint count = 0;

const unsigned long saveInterval = 5000;
const unsigned long debounceInterval = 50;
unsigned long lastDebounceTime = 0;

unsigned long lastSaveTime = 0;
volatile bool lastState = LOW;

void setup() {
  Serial.begin(115200);
  preferences.begin("barrel-count", false);
  count = preferences.getUInt("count", 0);

  WiFi.mode(WIFI_STA);
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setConfigPortalTimeout(180);

  if (wifiManager.autoConnect("BarrelCounter")) {
    Serial.println("Connected to WiFi");
  } else {
    Serial.println("Config Portal Running");
  }

  // Configure time for GMT+7 (Jakarta)
  const long gmtOffset_sec = 7 * 3600;  // 7 hours in seconds
  const int daylightOffset_sec = 0;     // Jakarta doesn't observe DST
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org");

  // Setup the interrupt for the button on pin 26
  pinMode(SWITCH_PIN, INPUT_PULLDOWN);

  // Initialize the LCD
  LCD.init();
  LCD.backlight();
}

void loop() {
  wifiManager.process();

  // Check if the button is pressed
  bool currentState = digitalRead(SWITCH_PIN);
  if (millis() - lastDebounceTime > debounceInterval) {
    if (currentState != lastState) {
      if (currentState == HIGH) {
        count++;
      }
      lastState = currentState;
      lastDebounceTime = millis();
    }
  }

  // Get local time from the RTC
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeString[20];
    // Format: dd/mm/yyyy hh:mm:ss
    sprintf(timeString, "%02d/%02d/%04d %02d:%02d:%02d", timeinfo.tm_mday,
            timeinfo.tm_mon + 1, timeinfo.tm_year + 1900, timeinfo.tm_hour,
            timeinfo.tm_min, timeinfo.tm_sec);
    LCD.setCursor(0, 0);
    LCD.print(timeString);
  } else {
    LCD.setCursor(0, 0);
    LCD.print("No time available ");
  }

  // Display the count on the second row
  LCD.setCursor(0, 1);
  LCD.print("BARREL: ");
  LCD.print(count);

  // Save the count to the preferences every 5 seconds
  unsigned long currentTime = millis();
  if (currentTime - lastSaveTime > saveInterval) {
    preferences.putUInt("count", count);
    lastSaveTime = currentTime;
    Serial.printf("Saved count: %d\n", count);
  }
}