#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <time.h>

#include <config.h>

void setup() {
  Serial.begin(115200);
  
  if (ACTIVE_LOW_SWITCH) { 
    pinMode(SWITCH_PIN_1, INPUT_PULLUP); 
    pinMode(SWITCH_PIN_2, INPUT_PULLUP); 
#ifdef DEBUG
    Serial.println("Switches configured for ACTIVE LOW (INPUT_PULLUP)");
#endif
  } else { 
    pinMode(SWITCH_PIN_1, INPUT_PULLDOWN); 
    pinMode(SWITCH_PIN_2, INPUT_PULLDOWN);
#ifdef DEBUG
    Serial.println("Switches configured for ACTIVE HIGH (INPUT_PULLDOWN)");
#endif
  }

  RTC_Init();
  LCD_Init();
  SD_Init();
  _currentDate = RTC_getTime();
  Preferences_Init(); 
  Webserver_Init();

  if (_lastTimeCheck == 0) {
      _lastTimeCheck = millis();
      _lastCountCheck = _count;
  }
}

void loop() {
  Webserver_Loop();
  
  Read_Switches(debounceInterval, ACTIVE_LOW_SWITCH); 

  Update_Running_Averages();

  String formattedTime;
  static unsigned long lastTimeUpdate = 0;
  unsigned long nowMillis = millis();

  if (nowMillis - lastTimeUpdate >= 1000) {
    lastTimeUpdate = nowMillis;
    _currentDate = RTC_getTime();
    char buf2[] = "DD-MM-YY hh:mm";
    char buf1[] = "YYYY-MM-DDThh:mm:ss"; 
    formattedTime = _currentDate.toString(buf2);
    String formattedTimeISO = _currentDate.toString(buf1);
    Send_Event(timeEvents, formattedTimeISO);

    if (_currentDate.day() != _lastDate.day()) {
      Reset_Count();
      _lastDate = _currentDate;
#ifdef DEBUG
      Serial.println("New day detected. Count and averages reset.");
#endif
    }
  }

  LCD.setCursor(0, 0);
  LCD.print(formattedTime); 
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