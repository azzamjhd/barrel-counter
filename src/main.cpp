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
  RTC_Init();
  LCD_Init();
  SD_Init();
  _currentDate = RTC_getTime();
  Preferences_Init();
  Webserver_Init();
}

void loop() {
  Webserver_Loop();
  // Check if the button is pressed
  Read_Switch(50, INPUT_HIGH);
  Calculate_Count_Per_Hour();
  String formattedTime, formattedTime2;

  static unsigned long lastTimeUpdate = 0;
  unsigned long nowMillis = millis();
  if (nowMillis - lastTimeUpdate >= 1000) {
    lastTimeUpdate = nowMillis;
    _currentDate = RTC_getTime();
    char buf2[] = "DD-MM-YY hh:mm";
    char buf1[] = "YYYY-MM-DDThh:mm:ss";
    formattedTime = _currentDate.toString(buf2);
    formattedTime2 = _currentDate.toString(buf1);
    timeEvents.send(formattedTime2.c_str());
    // Serial.println(_currentDate.timestamp());

    // Reset the counter each day
    if (_currentDate.day() != _lastDate.day()) {
      Reset_Count();
      _lastDate = _currentDate;
    }
  }

  // LCD.clear();
  // String currentDateTime = getCurrentLocalTime(true);
  LCD.setCursor(0, 0);
  LCD.print(formattedTime);
  // Display the _count on the second row
  LCD.setCursor(0, 1);
  LCD.print("C:");
  LCD.print(_count);
  LCD.print(" R:");
  LCD.print(_calcCountMin);
  LCD.print("m");
  LCD.print(" ");
  LCD.print(_calcCountHour);
  LCD.print("h");

  // Save the _count to the preferences every 5 seconds
  Save_To_Preferences(5000);
  Log_SD(Log_Interval * 1000);
}