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
// Renamed the event source to reflect running average
#define EVENT_SOURCE_RUNNING_AVERAGE "/runningAverageStream"
#define EVENT_SOURCE_TIME "/timeStream"
#define DNS_PORT 53
const IPAddress apIP(192, 168, 2, 1);
const IPAddress gateway(255, 255, 255, 0);
#define DEFAULT_AP_SSID "Drum Counter"
#define DEFAULT_AP_PASSWORD ""

#define SWITCH_PIN 26
#define PREFERENCES_KEY_NAME "count"
#define INPUT_HIGH true

const unsigned long saveInterval = 5000;
const unsigned long debounceInterval = 500;
// Configure time for GMT+7 (Jakarta)
const long gmtOffset_sec = 7 * 3600;  // 7 hours in seconds
const int daylightOffset_sec = 0;     // Jakarta doesn't observe DST

// Define smoothing factors for Exponential Moving Average
// Alpha = 1 / N, where N is the effective window size in seconds
const double ALPHA_CPM = 1.0 / 60.0;   // Smoothing over roughly 60 seconds
const double ALPHA_CPH = 1.0 / 3600.0; // Smoothing over roughly 3600 seconds

extern DNSServer dnsServer;
extern AsyncWebServer server;
extern AsyncEventSource countEvents;
// Use the new event source for running average
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
extern ulong _lastDebounceTime;
extern bool _lastState;

extern DateTime _currentDate;
extern DateTime _lastDate;

// Variables for running average calculation
extern ulong _lastTimeCheck; // Last time we checked count for rate calculation
extern uint _lastCountCheck; // Count at the last time check
extern double _runningAverageCPM; // Running average for counts per minute
extern double _runningAverageCPH; // Running average for counts per hour


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
void createDir(fs::FS &fs, const char * path);
void removeDir(fs::FS &fs, const char * path);
void readFile(fs::FS &fs, const char * path);
void writeFile(fs::FS &fs, const char * path, const char * message);
void appendFile(fs::FS &fs, const char * path, const char * message);
void renameFile(fs::FS &fs, const char * path1, const char * path2);
void deleteFile(fs::FS &fs, const char * path);

void Log_SD(ulong interval);

void Read_Switch(ulong debounceInterval, bool activeHigh);
void Update_Running_Averages();
void Reset_Count();
#endif