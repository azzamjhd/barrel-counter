#include <config.h>

DNSServer dnsServer;
AsyncWebServer server(80);
AsyncEventSource countEvents(EVENT_SOURCE_COUNT);
AsyncEventSource runningAverageEvents(EVENT_SOURCE_RUNNING_AVERAGE);
AsyncEventSource timeEvents(EVENT_SOURCE_TIME);
LiquidCrystal_I2C LCD(0x27, 16, 2);
Preferences preferences;
RTC_DS3231 rtc;

String STA_SSID = "your_ssid";
String STA_PASSWORD = "your_password";
String AP_SSID = "Drum Counter";
String AP_PASSWORD = "";
int Log_Interval = 60; // in seconds
ulong _lastLogTime = 0;
uint _lastLogCount = 0;

volatile uint _count = 0;
ulong _lastSaveTime = 0;

uint8_t debouncingStatePin1 = HIGH;
uint8_t debouncedStatePin1 = HIGH;
unsigned long lastDebounceTimePin1 = 0;

uint8_t debouncingStatePin2 = HIGH;
uint8_t debouncedStatePin2 = HIGH;
unsigned long lastDebounceTimePin2 = 0;

DateTime _currentDate;
DateTime _lastDate;

ulong _lastTimeCheck = 0;
uint _lastCountCheck = 0;
double _runningAverageCPM = 0.0;
double _runningAverageCPH = 0.0;

// Schedule variables - Default to schedule disabled, 07:00-16:00 if enabled
bool scheduleEnabled = false;
int startHour = 7;
int startMinute = 0;
int stopHour = 16;
int stopMinute = 0;

void redirectToIndex(AsyncWebServerRequest *request)
{
  request->redirect("http://" + apIP.toString());
}

void WiFi_Init()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_AP);
  if (AP_PASSWORD == "")
  {
    WiFi.softAP(AP_SSID.c_str());
  }
  else
  {
    WiFi.softAP(AP_SSID.c_str(), AP_PASSWORD.c_str());
  }
  WiFi.softAPConfig(apIP, apIP, gateway);

#ifdef DEBUG
  Serial.println("\nWiFi AP is now running\nIP address: ");
  Serial.println(WiFi.softAPIP());
#endif
}

void WiFi_Connect()
{
  int _attempts = 0;
#ifdef DEBUG
  Serial.println("Connecting to WiFi...");
#endif
  WiFi.begin(STA_SSID.c_str(), STA_PASSWORD.c_str());
  while (WiFi.status() != WL_CONNECTED && _attempts < 10)
  {
    delay(500);
#ifdef DEBUG
    Serial.print(".");
#endif
  }

  if (WiFi.status() == WL_CONNECTED)
  {
#ifdef DEBUG
    Serial.println("\nConnected to WiFi");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
#endif
  }
  else
  {
#ifdef DEBUG
    Serial.println("\nFailed to connect to WiFi");
#endif
  }
}

void Webserver_Init()
{
  WiFi_Init();

  if (!LittleFS.begin())
  {
#ifdef DEBUG
    Serial.println("An Error has occurred while mounting LittleFS");
#endif
    return;
  }
  server.addHandler(&countEvents);
  server.addHandler(&runningAverageEvents);
  server.addHandler(&timeEvents);
  Webserver_Routes();
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  server.serveStatic("/data", SD, "/").setCacheControl("max-age=60");
  server.begin();
#ifdef DEBUG
  Serial.println("Server Started");
#endif
}
// MARK: Webserver_Routes
void Webserver_Routes()
{
  server.on("/updateTime", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
            {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, data, len);
      if (!error) {
        long timeValue = doc["time"];
        rtc.adjust(DateTime(timeValue));
        request->send(200, "text/plain", "OK");
      } else {
        request->send(400, "text/plain", "Bad Request: JSON parse error");
      } });
  server.on("/wifiSetting", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
            {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, data, len);
      if (!error) {
        String ssid = doc["ssid"].as<String>();
        String password = doc["password"].as<String>();
        AP_SSID = ssid;
        AP_PASSWORD = password;
        preferences.putString("ssid", ssid);
        preferences.putString("password", password);
        request->send(200, "text/plain", "OK");
      } else {
        request->send(400, "text/plain", "Bad Request: JSON parse error");
      } });
  server.on("/getCount", HTTP_GET, [](AsyncWebServerRequest *request)
            {
      String count = String(_count);
      request->send(200, "text/plain", count); });
  server.on("/resetCount", HTTP_GET, [](AsyncWebServerRequest *request)
            {
      Reset_Count();
      request->send(200, "text/plain", "OK"); });
  server.on("/deleteFile", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
            { 
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, data, len);
      if (!error && doc.containsKey("file")) {
        String fileName = doc["file"].as<String>();
        // Ensure the fileName is an absolute path for SD.remove
        if (!fileName.startsWith("/")) {
            fileName = "/" + fileName;
        }
        if (SD.remove(fileName)) { // SD.remove expects absolute path
            request->send(200, "text/plain", "File deleted: " + fileName);
        } else {
            request->send(500, "text/plain", "Failed to delete file: " + fileName);
        }
      } else {
        request->send(400, "text/plain", "Bad Request: Missing 'file' in JSON body or parse error.");
      } });
  server.on("/listFiles", HTTP_GET, [](AsyncWebServerRequest *request)
            {
      String path = "/"; // Default to root directory
      if (request->hasParam("path")) {
        path = request->getParam("path")->value();
        if (!path.startsWith("/")) { // Ensure path is absolute
            path = "/" + path;
        }
      }
#ifdef DEBUG
      Serial.printf("Listing directory: %s\n", path.c_str());
#endif
      String jsonString = listDir(SD, path.c_str(), 2); // List 2 levels deep
      request->send(200, "application/json", jsonString); });
  server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request)
            {
      request->send(200, "text/plain", "OK");
      delay(1000);
      ESP.restart(); });

  server.on("/setSchedule", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
            {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
      request->send(400, "text/plain", "Bad Request: JSON parse error");
      return;
    }

    bool reqEnabled = doc["enabled"] | scheduleEnabled; 
    int reqStartHour = doc["startHour"] | startHour;
    int reqStartMinute = doc["startMinute"] | startMinute;
    int reqStopHour = doc["stopHour"] | stopHour;
    int reqStopMinute = doc["stopMinute"] | stopMinute;

    if (reqStartHour < 0 || reqStartHour > 23 || reqStartMinute < 0 || reqStartMinute > 59 ||
        reqStopHour < 0 || reqStopHour > 23 || reqStopMinute < 0 || reqStopMinute > 59) {
      request->send(400, "text/plain", "Bad Request: Invalid time values.");
      return;
    }

    scheduleEnabled = reqEnabled;
    startHour = reqStartHour;
    startMinute = reqStartMinute;
    stopHour = reqStopHour;
    stopMinute = reqStopMinute;

    preferences.putBool(PREF_KEY_SCH_ENABLED, scheduleEnabled);
    preferences.putInt(PREF_KEY_SCH_START_H, startHour);
    preferences.putInt(PREF_KEY_SCH_START_M, startMinute);
    preferences.putInt(PREF_KEY_SCH_STOP_H, stopHour);
    preferences.putInt(PREF_KEY_SCH_STOP_M, stopMinute);

#ifdef DEBUG
    Serial.println("Schedule updated via web:");
    Serial.printf("Enabled: %s, Start: %02d:%02d, Stop: %02d:%02d\n",
                  scheduleEnabled ? "true" : "false", startHour, startMinute, stopHour, stopMinute);
#endif
    request->send(200, "text/plain", "OK"); });
  server.on("/getSchedule", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    JsonDocument doc;
    doc["enabled"] = scheduleEnabled;
    doc["startHour"] = startHour;
    doc["startMinute"] = startMinute;
    doc["stopHour"] = stopHour;
    doc["stopMinute"] = stopMinute;

    String jsonString;
    serializeJson(doc, jsonString);
    request->send(200, "application/json", jsonString); });
}

void Webserver_Loop()
{
  // dnsServer.processNextRequest(); // Typically not needed with ESPAsyncWebServer
}

void Send_Event(AsyncEventSource &eventSource, const String &eventData)
{
  eventSource.send(eventData.c_str());
}
// MARK: LCD_Init
void LCD_Init()
{
  LCD.init();
  LCD.backlight();
}
// MARK: Preferences_Init
void Preferences_Init()
{
  preferences.begin("barrel");
  _count = preferences.getUInt(PREFERENCES_KEY_NAME, 0);
  long unix = preferences.getLong("lastDate", _currentDate.unixtime());
  _lastDate = DateTime(unix);

  _runningAverageCPM = preferences.getDouble("avgCPM", 0.0);
  _runningAverageCPH = preferences.getDouble("avgCPH", 0.0);
  _lastTimeCheck = preferences.getULong("lastTimeCheck", 0);
  _lastCountCheck = preferences.getUInt("lastCountCheck", 0);
  _lastLogCount = preferences.getUInt("lastLogCount", 0);
  AP_SSID = preferences.getString("ssid", DEFAULT_AP_SSID);
  AP_PASSWORD = preferences.getString("password", DEFAULT_AP_PASSWORD);

  scheduleEnabled = preferences.getBool(PREF_KEY_SCH_ENABLED, false);
  startHour = preferences.getInt(PREF_KEY_SCH_START_H, 7);
  startMinute = preferences.getInt(PREF_KEY_SCH_START_M, 0);
  stopHour = preferences.getInt(PREF_KEY_SCH_STOP_H, 16);
  stopMinute = preferences.getInt(PREF_KEY_SCH_STOP_M, 0);

#ifdef DEBUG
  Serial.println("Loaded schedule settings from Preferences:");
  Serial.printf("Enabled: %s, Start: %02d:%02d, Stop: %02d:%02d\n",
                scheduleEnabled ? "true" : "false", startHour, startMinute, stopHour, stopMinute);
#endif

  if (_lastTimeCheck == 0)
  {
    _lastTimeCheck = millis();
    _lastCountCheck = _count;
  }
}
// MARK: Save_To_Preferences
void Save_To_Preferences(ulong interval)
{
  unsigned long currentTime = millis();
  if (currentTime - _lastSaveTime > interval)
  {
    preferences.putUInt(PREFERENCES_KEY_NAME, _count);
    preferences.putLong("lastDate", _currentDate.unixtime());
    preferences.putDouble("avgCPM", _runningAverageCPM);
    preferences.putDouble("avgCPH", _runningAverageCPH);
    preferences.putULong("lastTimeCheck", _lastTimeCheck);
    preferences.putUInt("lastCountCheck", _lastCountCheck);
    preferences.putUInt("lastLogCount", _lastLogCount);
    preferences.putString("ssid", AP_SSID);
    preferences.putString("password", AP_PASSWORD);
    preferences.putBool(PREF_KEY_SCH_ENABLED, scheduleEnabled);
    preferences.putInt(PREF_KEY_SCH_START_H, startHour);
    preferences.putInt(PREF_KEY_SCH_START_M, startMinute);
    preferences.putInt(PREF_KEY_SCH_STOP_H, stopHour);
    preferences.putInt(PREF_KEY_SCH_STOP_M, stopMinute);
    _lastSaveTime = currentTime;
#ifdef DEBUG
    // ESP_LOGI("PREFERENCES", "Saved all settings to preferences."); // Use Serial.println for consistency if ESP_LOG not configured
    Serial.println("Saved all settings to preferences.");
#endif
  }
}
// MARK: Reset_Count
void Reset_Count()
{
  _count = 0;
  _runningAverageCPM = 0.0;
  _runningAverageCPH = 0.0;
  _lastCountCheck = 0;
  _lastTimeCheck = millis();
  _lastLogCount = 0;

  preferences.putUInt(PREFERENCES_KEY_NAME, _count);
  preferences.putDouble("avgCPM", _runningAverageCPM);
  preferences.putDouble("avgCPH", _runningAverageCPH);
  preferences.putUInt("lastCountCheck", _lastCountCheck);
  preferences.putULong("lastTimeCheck", _lastTimeCheck);
  preferences.putUInt("lastLogCount", _lastLogCount);

  Send_Event(countEvents, String(_count));
  String avgData = String(_runningAverageCPM) + "," + String(_runningAverageCPH);
  Send_Event(runningAverageEvents, avgData);

#ifdef DEBUG
  Serial.println("Count reset to " + String(_count));
#endif
}
// MARK: Update_Running_Averages
void Update_Running_Averages()
{
  unsigned long currentTime = millis();
  const unsigned long updateInterval = 1000;

  if (currentTime - _lastTimeCheck >= updateInterval)
  {
    unsigned long deltaTime = currentTime - _lastTimeCheck;
    uint countIncrease = _count - _lastCountCheck;

    if (deltaTime > 0)
    {
      double currentRatePerMs = (double)countIncrease / deltaTime;
      double currentRatePerMinute = currentRatePerMs * 1000.0 * 60.0;
      _runningAverageCPM = (ALPHA_CPM * currentRatePerMinute) + ((1.0 - ALPHA_CPM) * _runningAverageCPM);
      double currentRatePerHour = currentRatePerMs * 1000.0 * 3600.0;
      _runningAverageCPH = (ALPHA_CPH * currentRatePerHour) + ((1.0 - ALPHA_CPH) * _runningAverageCPH);

      if (_runningAverageCPM < 0)
        _runningAverageCPM = 0;
      if (_runningAverageCPH < 0)
        _runningAverageCPH = 0;

      String avgData = String(_runningAverageCPM) + "," + String(_runningAverageCPH);
      Send_Event(runningAverageEvents, avgData);
    }
    _lastTimeCheck = currentTime;
    _lastCountCheck = _count;
  }
}
// MARK: isTimeWithinScheduledRange
bool isTimeWithinScheduledRange(const DateTime &now)
{
  if (!scheduleEnabled)
  {
    return true;
  }

  int currentHour = now.hour();
  int currentMinute = now.minute();
  long currentTimeInMinutes = currentHour * 60 + currentMinute;
  long startTimeInMinutes = startHour * 60 + startMinute;
  long stopTimeInMinutes = stopHour * 60 + stopMinute;
  bool isActive;

  if (startTimeInMinutes <= stopTimeInMinutes)
  {
    isActive = (currentTimeInMinutes >= startTimeInMinutes && currentTimeInMinutes < stopTimeInMinutes);
  }
  else
  {
    isActive = (currentTimeInMinutes >= startTimeInMinutes || currentTimeInMinutes < stopTimeInMinutes);
  }
#ifdef DEBUG
  static bool lastReportedActiveStatus = !isActive;
  if (isActive != lastReportedActiveStatus || lastReportedActiveStatus == !isActive)
  { // Print on change or first check
    Serial.printf("Schedule check: Now %02d:%02d. Range %02d:%02d-%02d:%02d. Counting Active: %s\n",
                  currentHour, currentMinute, startHour, startMinute, stopHour, stopMinute, isActive ? "YES" : "NO");
    lastReportedActiveStatus = isActive;
  }
#endif
  return isActive;
}
// MARK: Read_Switches
void Read_Switches(ulong debounceInterval, bool isActiveLow)
{
  if (!isTimeWithinScheduledRange(_currentDate))
  {
    return;
  }

  unsigned long currentTime = millis();
  bool pin1_activated_this_cycle = false;
  bool pin2_activated_this_cycle = false;

  bool currentReadingPin1 = digitalRead(SWITCH_PIN_1);
  if (currentReadingPin1 != debouncingStatePin1)
  {
    lastDebounceTimePin1 = currentTime;
    debouncingStatePin1 = currentReadingPin1;
  }
  if ((currentTime - lastDebounceTimePin1) > debounceInterval)
  {
    if (debouncingStatePin1 != debouncedStatePin1)
    {
      debouncedStatePin1 = debouncingStatePin1;
      if (debouncedStatePin1 == (isActiveLow ? LOW : HIGH))
      {
        pin1_activated_this_cycle = true;
      }
    }
  }

  bool currentReadingPin2 = digitalRead(SWITCH_PIN_2);
  if (currentReadingPin2 != debouncingStatePin2)
  {
    lastDebounceTimePin2 = currentTime;
    debouncingStatePin2 = currentReadingPin2;
  }
  if ((currentTime - lastDebounceTimePin2) > debounceInterval)
  {
    if (debouncingStatePin2 != debouncedStatePin2)
    {
      debouncedStatePin2 = debouncingStatePin2;
      if (debouncedStatePin2 == (isActiveLow ? LOW : HIGH))
      {
        pin2_activated_this_cycle = true;
      }
    }
  }

  if (pin1_activated_this_cycle || pin2_activated_this_cycle)
  {
    _count++;
    Send_Event(countEvents, String(_count));
#ifdef DEBUG
    Serial.print("Count: " + String(_count) + " (Triggered by: ");
    if (pin1_activated_this_cycle)
      Serial.print("SW1 ");
    if (pin2_activated_this_cycle)
      Serial.print("SW2");
    Serial.println(")");
#endif
  }
}
// MARK: RTC_Init
void RTC_Init()
{
  if (!rtc.begin())
  {
    Serial.println("Couldn't find RTC");
    Serial.flush();
  }
  if (rtc.lostPower())
  {
    Serial.println("RTC lost power, setting the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  rtc.disable32K();
}

DateTime RTC_getTime()
{
  DateTime now = rtc.now();
  return now;
}
// MARK: SD_Init
void SD_Init()
{
  if (!SD.begin()) // Pass CS pin if not default, e.g., SD.begin(SS_PIN)
  {
#ifdef DEBUG
    Serial.println("Card Mount Failed");
#endif
    return;
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE)
  {
#ifdef DEBUG
    Serial.println("No SD card attached");
#endif
    return;
  }
#ifdef DEBUG
  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC)
    Serial.println("MMC");
  else if (cardType == CARD_SD)
    Serial.println("SDSC");
  else if (cardType == CARD_SDHC)
    Serial.println("SDHC");
  else
    Serial.println("Unknown type");
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
#endif
}
// MARK: listDir
String listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
  JsonDocument doc; // For ArduinoJson v6+

  File root = fs.open(dirname);
  if (!root)
  {
    root.close(); // Ensure closed even if open failed or was invalid
#ifdef DEBUG
    Serial.printf("Failed to open directory: %s\n", dirname);
#endif
    return "Failed to open directory";
  }
  if (!root.isDirectory())
  {
    root.close(); // Close the handle
#ifdef DEBUG
    Serial.printf("Not a directory: %s\n", dirname);
#endif
    return "Not a directory";
  }

  File file = root.openNextFile();
  while (file)
  {
    String entryName = file.name(); // This is just the name, e.g., "file.txt" or "subdir"
    String fullEntryPath;

    // Construct the full absolute path for the current entry
    if (String(dirname) == "/")
    {
      fullEntryPath = "/" + entryName;
    }
    else
    {
      String basePath = dirname;
      // Ensure basePath does not end with a slash if it's not the root, before appending another
      if (basePath.endsWith("/"))
      {
        basePath.remove(basePath.length() - 1);
      }
      fullEntryPath = basePath + "/" + entryName;
    }
    // Sanitize: remove any double slashes that might have formed (e.g. if dirname was "/" and somehow logic above failed)
    while (fullEntryPath.indexOf("//") != -1)
    {
      fullEntryPath.replace("//", "/");
    }

    if (file.isDirectory())
    {
#ifdef DEBUG
      Serial.printf("listDir: Dir: %s\n", fullEntryPath.c_str());
#endif
      if (levels > 0)
      {
        String childrenJsonString = listDir(fs, fullEntryPath.c_str(), levels - 1);
        JsonDocument childrenDoc;
        DeserializationError error = deserializeJson(childrenDoc, childrenJsonString);

        if (!error)
        {
          if (childrenDoc.is<JsonArray>())
          {
            for (JsonVariant child : childrenDoc.as<JsonArray>())
            {
              if (child.is<const char *>())
              { // Ensure it's a string before adding
                doc.add(child.as<String>());
              }
            }
          }
          else if (childrenDoc.containsKey("error"))
          {
#ifdef DEBUG
            Serial.printf("listDir: Error in subdirectory %s: %s\n", fullEntryPath.c_str(), childrenDoc["error"].as<const char *>());
#endif
          }
        }
        else
        {
#ifdef DEBUG
          Serial.printf("listDir: Failed to parse children JSON for %s: %s\n", fullEntryPath.c_str(), error.c_str());
#endif
        }
      }
    }
    else
    {
#ifdef DEBUG
      Serial.printf("listDir: File: %s, Size: %lu\n", fullEntryPath.c_str(), file.size());
#endif
      doc.add(fullEntryPath); // Add the full absolute path of the file
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();

  String jsonOutput;
  if (doc.size() == 0 && String(dirname) != "/" && !fs.exists(dirname))
  {
    // If we listed an empty directory that doesn't actually exist (e.g. after a failed fs.open on a non-existent path that somehow passed initial checks)
    // This case is less likely if initial open fails correctly.
    // However, if the directory is simply empty, an empty array is correct.
  }

  serializeJson(doc, jsonOutput);
  return jsonOutput;
}
// MARK: Log_SD
void Log_SD(ulong interval)
{
  unsigned long currentTime = millis();
  if (SD.cardType() != CARD_NONE && currentTime - _lastLogTime > interval && _count != _lastLogCount)
  {
    DateTime now = RTC_getTime();
    String filename = "/";                               // Logs are stored in the root
    filename += now.timestamp(DateTime::TIMESTAMP_DATE); // e.g., YYYY-MM-DD
    filename += ".csv";

    File dataFile;
    bool newFile = false;

    if (!SD.exists(filename.c_str()))
    {
      dataFile = SD.open(filename.c_str(), FILE_WRITE);
      if (dataFile)
      {
        dataFile.println("time,count,cpm,cph"); // Write header for new file
        newFile = true;
      }
      else
      {
#ifdef DEBUG
        Serial.println("Log_SD: Error creating file: " + filename);
#endif
        return;
      }
    }
    else
    {
      dataFile = SD.open(filename.c_str(), FILE_APPEND);
      if (!dataFile)
      {
#ifdef DEBUG
        Serial.println("Log_SD: Error opening file for append: " + filename);
#endif
        return;
      }
    }

    if (dataFile) // If file is open successfully
    {
      String logEntry = now.timestamp(DateTime::TIMESTAMP_FULL); // Full timestamp e.g., YYYY-MM-DDTHH:MM:SS
      logEntry += ",";
      logEntry += String(_count);
      logEntry += ",";
      logEntry += String(_runningAverageCPM, 2);
      logEntry += ",";
      logEntry += String(_runningAverageCPH, 2);
      dataFile.println(logEntry);
      dataFile.close();

#ifdef DEBUG
      Serial.printf("Logged to %s: %s (%s)\n", filename.c_str(), logEntry.c_str(), newFile ? "New file" : "Appended");
#endif
      _lastLogTime = currentTime;
      _lastLogCount = _count;
    }
  }
}
