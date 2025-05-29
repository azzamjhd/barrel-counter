#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>
#include <time.h>
#include <RTClib.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

#define DEBUG

#define EVENT_SOURCE_COUNT "/counterStream"
#define EVENT_SOURCE_RUNNING_AVERAGE "/runningAverageStream"
#define EVENT_SOURCE_TIME "/timeStream"
#define DNS_PORT 53
const IPAddress apIP(192, 168, 2, 1);
const IPAddress gateway(255, 255, 255, 0);
#define DEFAULT_AP_SSID "Drum Counter"
#define DEFAULT_AP_PASSWORD ""

#define SWITCH_PIN_1 25 // Limit switch 1 on GPIO 25
#define SWITCH_PIN_2 26 // Limit switch 2 on GPIO 26
#define PREFERENCES_KEY_NAME "count"
#define ACTIVE_LOW_SWITCH true // Set to true for active-low inputs, false for active-high

// Preference keys for schedule settings
#define PREF_KEY_SCH_ENABLED "schEnabled"
#define PREF_KEY_SCH_START_H "schStartH"
#define PREF_KEY_SCH_START_M "schStartM"
#define PREF_KEY_SCH_STOP_H "schStopH"
#define PREF_KEY_SCH_STOP_M "schStopM"


const unsigned long saveInterval = 5000;
const unsigned long debounceInterval = 500; // Milliseconds for switch debounce
const long gmtOffset_sec = 7 * 3600;  // 7 hours in seconds
const int daylightOffset_sec = 0;     // Jakarta doesn't observe DST

const double ALPHA_CPM = 1.0 / 60.0;
const double ALPHA_CPH = 1.0 / 3600.0;

extern DNSServer dnsServer;
extern AsyncWebServer server;
extern AsyncEventSource countEvents;
extern AsyncEventSource runningAverageEvents;
extern AsyncEventSource timeEvents;
extern LiquidCrystal_I2C LCD;
extern Preferences preferences;
extern RTC_DS3231 rtc;

extern String STA_SSID;
extern String STA_PASSWORD;
extern String AP_SSID;
extern String AP_PASSWORD;
extern int Log_Interval;
extern ulong _lastLogTime;
extern uint _lastLogCount;

extern volatile uint _count;
extern ulong _lastSaveTime;

extern uint8_t debouncingStatePin1;
extern uint8_t debouncedStatePin1;
extern unsigned long lastDebounceTimePin1;

extern uint8_t debouncingStatePin2;
extern uint8_t debouncedStatePin2;
extern unsigned long lastDebounceTimePin2;

extern DateTime _currentDate;
extern DateTime _lastDate;

extern ulong _lastTimeCheck;
extern uint _lastCountCheck;
extern double _runningAverageCPM;
extern double _runningAverageCPH;

// Schedule variables
extern bool scheduleEnabled; 
extern int startHour;      
extern int startMinute;    
extern int stopHour;       
extern int stopMinute;     

void redirectToIndex(AsyncWebServerRequest *request);
void WiFi_Init();
void WiFi_Connect();

void Webserver_Init();
void Webserver_Routes();
void Webserver_Loop();
void Send_Event(AsyncEventSource& eventSource, const String &eventData);

void LCD_Init();
void Preferences_Init();
void Save_To_Preferences(ulong interval);

void RTC_Init();
DateTime RTC_getTime();

void SD_Init();
String listDir(fs::FS &fs, const char * dirname, uint8_t levels);

void Log_SD(ulong interval);

void Read_Switches(ulong debounceInterval, bool isActiveLow);
void Update_Running_Averages();
void Reset_Count();

// Function to check if current time is within scheduled counting range
bool isTimeWithinScheduledRange(const DateTime& now);

#endif