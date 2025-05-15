#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <time.h>

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
  SD_Init();
  Webserver_Init();
  RTC_Init();
}

void loop() {
  Webserver_Loop();
  // Check if the button is pressed
  Read_Switch(50, INPUT_HIGH);
  String formattedTime;

  static unsigned long lastTimeUpdate = 0;
  unsigned long nowMillis = millis();
  if (nowMillis - lastTimeUpdate >= 1000) {
    lastTimeUpdate = nowMillis;
    _currentDate = RTC_getTime();
    char buf2[] = "YY/MM/DD-hh:mm:ss";
    formattedTime = _currentDate.toString(buf2);
    // Serial.println(_currentDate.timestamp());

    // Reset the counter each day
    if (_currentDate.day() != _lastDate.day()) {
      Reset_Count();
      _lastDate = _currentDate;
    }
  }

  // String currentDateTime = getCurrentLocalTime(true);
  LCD.setCursor(0, 0);
  LCD.print(formattedTime);
  // Display the _count on the second row
  LCD.setCursor(0, 1);
  LCD.print("BARREL: ");
  LCD.print(_count);

  // Save the _count to the preferences every 5 seconds
  Save_To_Preferences(5000);
  Log_SD(Log_Interval * 1000);
}