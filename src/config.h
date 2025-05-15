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

#define EVENT_SOURCE "/counterStream"
#define DNS_PORT 53
const IPAddress apIP(192, 168, 2, 1);
const IPAddress gateway(255, 255, 255, 0);

#define SWITCH_PIN 26
#define PREFERENCES_KEY_NAME "count"
#define INPUT_HIGH true

const unsigned long saveInterval = 5000;
const unsigned long debounceInterval = 50;
// Configure time for GMT+7 (Jakarta)
const long gmtOffset_sec = 7 * 3600;  // 7 hours in seconds
const int daylightOffset_sec = 0;     // Jakarta doesn't observe DST

extern DNSServer dnsServer;
extern AsyncWebServer server;
extern AsyncEventSource events;
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

void redirectToIndex(AsyncWebServerRequest *request);
void WiFi_Init();
void WiFi_Connect();
String WiFi_Scan();

void Webserver_Init();
void Webserver_Routes();
void Webserver_Loop();
void Send_Event(const String &eventName, const String &eventData);

void LCD_Init();
void Preferences_Init();
void Save_To_Preferences(ulong interval);
String getCurrentLocalTime(bool showDate);

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
void Reset_Count();
#endif