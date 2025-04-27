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

#define DEBUG

#define SSID "Drum Counter"
#define PASSWORD "12345678"
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

extern volatile uint _count;
extern ulong _lastSaveTime;
extern ulong _lastDebounceTime;
extern bool _lastState;

void redirectToIndex(AsyncWebServerRequest *request);
void Webserver_Init();
void Webserver_Loop();
void Send_Event(const String &eventName, const String &eventData);

void LCD_Init();
void Preferences_Init();
void Save_To_Preferences(ulong interval);
void Read_Switch(ulong debounceInterval, bool activeHigh);
String getCurrentLocalTime(bool showDate);


#endif