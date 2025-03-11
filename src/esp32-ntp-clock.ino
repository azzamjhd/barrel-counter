#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Wire.h>
#include <time.h>

#define SWITCH_PIN 26

Preferences preferences;
LiquidCrystal_I2C LCD(0x27, 16, 2);

volatile uint count = 0;
volatile bool alreadyCounted = false;
volatile unsigned long lastInterruptTime = 0;
volatile unsigned long lastSaveTime = 0;
volatile bool buttonWasOff = true;

const unsigned long debounceDelay = 100;
const unsigned long saveInterval = 5000;

// Button interrupt function to count events
void IRAM_ATTR countup() {
  unsigned long interruptTime = millis();
  // Debounce: ignore if within debounce delay
  if (interruptTime - lastInterruptTime < debounceDelay) return;
  lastInterruptTime = interruptTime;

  bool currentState = digitalRead(SWITCH_PIN);
  if (currentState && buttonWasOff) { // Switch went HIGH and was previously off
    count++;
    buttonWasOff = false; // Button is now on
  } else if (!currentState) { // Switch went LOW
    buttonWasOff = true; // Button is now off
  }
}

void setup() {
  Serial.begin(115200);
  preferences.begin("barrel-count", false);
  count = preferences.getUInt("count", 0);

  // Connect to WiFi for NTP synchronization
  WiFi.begin("Wokwi-GUEST", "");
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");

  // Configure time for GMT+7 (Jakarta)
  const long gmtOffset_sec = 7 * 3600;  // 7 hours in seconds
  const int daylightOffset_sec = 0;     // Jakarta doesn't observe DST
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org");

  // Setup the interrupt for the button on pin 26
  pinMode(SWITCH_PIN, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(SWITCH_PIN), countup, CHANGE);

  // Initialize the LCD
  LCD.init();
  LCD.backlight();
}

void loop() {
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
  }
}