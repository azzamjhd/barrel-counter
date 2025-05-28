#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <time.h>

#include <config.h>

void setup() {
  Serial.begin(115200);
  // Setup the interrupt for the button on pin 26
  Switch_Init(INPUT_HIGH);
  RTC_Init();
  LCD_Init();
  SD_Init();
  _currentDate = RTC_getTime();
  Preferences_Init(); // This will load saved running averages and last check time/count
  Webserver_Init();

  // Initialize _lastTimeCheck and _lastCountCheck if they were not loaded from preferences (first boot)
  if (_lastTimeCheck == 0) {
      _lastTimeCheck = millis();
      _lastCountCheck = _count;
  }
}

void loop() {
  Webserver_Loop();
  // Check if the button is pressed
  Read_Switch(debounceInterval, INPUT_HIGH); // Use debounceInterval from config.h

  // Update running averages periodically
  Update_Running_Averages();

  String formattedTime;

  static unsigned long lastTimeUpdate = 0;
  unsigned long nowMillis = millis();
  // Update time and check for date change every second
  if (nowMillis - lastTimeUpdate >= 1000) {
    lastTimeUpdate = nowMillis;
    _currentDate = RTC_getTime();
    char buf2[] = "DD-MM-YY hh:mm";
    char buf1[] = "YYYY-MM-DDThh:mm:ss"; // ISO 8601 format for web events
    formattedTime = _currentDate.toString(buf2);
    String formattedTimeISO = _currentDate.toString(buf1);
    Send_Event(timeEvents, formattedTimeISO);
    // Serial.println(_currentDate.timestamp());

    // Reset the counter each day
    if (_currentDate.day() != _lastDate.day()) {
      Reset_Count();
      _lastDate = _currentDate;
    }
  }

  // LCD.clear(); // Clearing the LCD every loop can cause flicker
  LCD.setCursor(0, 0);
  LCD.print(formattedTime);
  // Display the _count and running averages on the second row
  LCD.setCursor(0, 1);
  String counterText = "C: ";
  counterText += String(_count);
  counterText += " R: ";
  counterText += String(_runningAverageCPM, 1);
  counterText += "m ";
  counterText += String(_runningAverageCPH, 1);
  counterText += "h";
  LCD.print(counterText);

  // Save the _count and running averages to the preferences every saveInterval
  Save_To_Preferences(saveInterval); // Use saveInterval from config.h
  // Log data to SD card periodically
  Log_SD(Log_Interval * 1000); // Use Log_Interval from config.h
}