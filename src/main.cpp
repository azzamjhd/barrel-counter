#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <time.h>
#include <ESPAsyncWebServer.h>

#include <config.h>

unsigned long lastDebounceTime = 0;
unsigned long lastSaveTime = 0;
volatile bool lastState = LOW;

void setup() {
  Serial.begin(115200);
  // Setup the interrupt for the button on pin 26
  if (INPUT_HIGH) {
    pinMode(SWITCH_PIN, INPUT_PULLDOWN);
  } else {
    pinMode(SWITCH_PIN, INPUT_PULLUP);
  }
  Preferences_Init();
  LCD_Init();

  Webserver_Init();

  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org");
}

void loop() {
  Webserver_Loop();
  // Check if the button is pressed
  Read_Switch(50, INPUT_HIGH);

  // String currentDateTime = getCurrentLocalTime(true);
  // LCD.setCursor(0, 0);
  // LCD.print(currentDateTime);
  // // Display the _count on the second row
  LCD.setCursor(0, 1);
  LCD.print("BARREL: ");
  LCD.print(_count);

  // Save the _count to the preferences every 5 seconds
  // Save_To_Preferences(5000);
}