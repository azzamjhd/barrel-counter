#include <config.h>

DNSServer dnsServer;
AsyncWebServer server(80);
AsyncEventSource countEvents(EVENT_SOURCE_COUNT);
// Use the new event source name
AsyncEventSource runningAverageEvents(EVENT_SOURCE_RUNNING_AVERAGE);
AsyncEventSource timeEvents(EVENT_SOURCE_TIME);
LiquidCrystal_I2C LCD(0x27, 16, 2);
Preferences preferences;
RTC_DS3231 rtc;

String STA_SSID = "your_ssid";
String STA_PASSWORD = "your_password";
String AP_SSID = "Drum Counter";
String AP_PASSWORD = "12345678";
int Log_Interval = 60; // in seconds
ulong _lastLogTime = 0;
uint _lastLogCount = 0; // Variable to store the count at the last log

volatile uint _count = 0;
ulong _lastSaveTime = 0;
ulong _lastDebounceTime = 0;
bool _lastState = 0;
DateTime _currentDate;
DateTime _lastDate;

// Variables for running average calculation
ulong _lastTimeCheck = 0;        // Last time we checked count for rate calculation
uint _lastCountCheck = 0;        // Count at the last time check
double _runningAverageCPM = 0.0; // Running average for counts per minute
double _runningAverageCPH = 0.0; // Running average for counts per hour

void redirectToIndex(AsyncWebServerRequest *request)
{
  request->redirect("http://" + apIP.toString());
}

void WiFi_Init()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  // WiFi.mode(WIFI_AP_STA);
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
  // dnsServer.start(DNS_PORT, "*", apIP);
  // WiFi_Connect();

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

// @note this function is called in the setup() function
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
  // server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
  //   request->send(204);
  // });
  server.addHandler(&countEvents);
  // Add the new event source handler
  server.addHandler(&runningAverageEvents);
  server.addHandler(&timeEvents);
  Webserver_Routes();
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  server.serveStatic("/data", SD, "/").setCacheControl("max-age=60");
  // server.onNotFound(redirectToIndex);
  server.begin();
#ifdef DEBUG
  Serial.println("Server Started");
#endif
}

void Webserver_Routes()
{
  server.on("/updateTime", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
            {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, data, len);
      if (!error) {
        long timeValue = doc["time"];
#ifdef DEBUG
        Serial.println("Time: " + String(timeValue));
#endif
        rtc.adjust(DateTime(timeValue));
        request->send(200, "text/plain", "OK");
      } else {
        Serial.println("JSON parse error");
        request->send(400, "text/plain", "Bad Request");
      } });
  server.on("/wifiSetting", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
            {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, data, len);
      if (!error) {
        String ssid = doc["ssid"].as<String>();
        String password = doc["password"].as<String>();
#ifdef DEBUG
        Serial.println("Received WiFi settings:");
        Serial.println("SSID: " + ssid);
        Serial.println("Password: " + password);
#endif
        AP_SSID = ssid;
        AP_PASSWORD = password;
        preferences.putString("ssid", ssid);
        preferences.putString("password", password);
        request->send(200, "text/plain", "OK");
      } else {
        Serial.println("JSON parse error");
        request->send(400, "text/plain", "Bad Request");
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
      if (request->hasParam("file")) {
        String fileName = request->getParam("file")->value();
        if (SD.remove(fileName)) request->send(200, "text/plain", "File deleted");
        else request->send(500, "text/plain", "Failed to delete file");
      }
      else request->send(400, "text/plain", "Bad Request"); });
  server.on("/listFiles", HTTP_GET, [](AsyncWebServerRequest *request)
            {
      String path = "/";
      if (request->hasParam("path")) {
        path = request->getParam("path")->value();
      }
      String jsonString = listDir(SD, path.c_str(), 2);
      request->send(200, "application/json", jsonString); });
  server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request)
            {
      request->send(200, "text/plain", "OK");
      delay(1000);
      ESP.restart(); });
}
// @note this function is called in the loop() function
void Webserver_Loop()
{
  // dnsServer.processNextRequest();
}
// @param eventSource the AsyncEventSource to send the event to
// @param eventData data to send with the event
// @note the event name should be unique for each event
void Send_Event(AsyncEventSource &eventSource, const String &eventData)
{
  eventSource.send(eventData.c_str());
}

void LCD_Init()
{
  LCD.init();
  LCD.backlight();
}

void Preferences_Init()
{
  preferences.begin("barrel");
  _count = preferences.getUInt(PREFERENCES_KEY_NAME, 0);
  long unix = preferences.getLong("lastDate", _currentDate.unixtime());
  _lastDate = DateTime(unix);

  // Load running average values from preferences
  _runningAverageCPM = preferences.getDouble("avgCPM", 0.0);
  _runningAverageCPH = preferences.getDouble("avgCPH", 0.0);
  _lastTimeCheck = preferences.getULong("lastTimeCheck", 0);
  _lastCountCheck = preferences.getUInt("lastCountCheck", 0);
  _lastLogCount = preferences.getUInt("lastLogCount", 0);
  AP_SSID = preferences.getString("ssid", DEFAULT_AP_SSID);
  AP_PASSWORD = preferences.getString("password", DEFAULT_AP_PASSWORD);

  // If _lastTimeCheck is 0 (first boot or after reset), initialize it
  if (_lastTimeCheck == 0)
  {
    _lastTimeCheck = millis();
    _lastCountCheck = _count;
  }
}
// @param interval save interval to preferences in millisecond
void Save_To_Preferences(ulong interval)
{
  unsigned long currentTime = millis();
  if (currentTime - _lastSaveTime > interval)
  {
    preferences.putUInt(PREFERENCES_KEY_NAME, _count);
    preferences.putLong("lastDate", _currentDate.unixtime());

    // Save running average values to preferences
    preferences.putDouble("avgCPM", _runningAverageCPM);
    preferences.putDouble("avgCPH", _runningAverageCPH);
    preferences.putULong("lastTimeCheck", _lastTimeCheck);
    preferences.putUInt("lastCountCheck", _lastCountCheck);
    preferences.putUInt("lastLogCount", _lastLogCount);
    preferences.putString("ssid", AP_SSID);
    preferences.putString("password", AP_PASSWORD);

    _lastSaveTime = currentTime;
    ESP_LOGI("BARREL_COUNTER", "Saved _count: %d", _count);
    ESP_LOGI("BARREL_COUNTER", "Saved avgCPM: %.2f, avgCPH: %.2f", _runningAverageCPM, _runningAverageCPH);
    ESP_LOGI("BARREL_COUNTER", "Saved lastLogCount: %d", _lastLogCount);
  }
}
// @note reset _count and preferences count, and running averages
void Reset_Count()
{
  _count = 0;
  _runningAverageCPM = 0.0;
  _runningAverageCPH = 0.0;
  _lastCountCheck = 0;
  _lastTimeCheck = millis(); // Reset time check on count reset
  _lastLogCount = 0;         // Reset last logged count on total count reset

  preferences.putUInt(PREFERENCES_KEY_NAME, _count);
  preferences.putDouble("avgCPM", _runningAverageCPM);
  preferences.putDouble("avgCPH", _runningAverageCPH);
  preferences.putUInt("lastCountCheck", _lastCountCheck);
  preferences.putULong("lastTimeCheck", _lastTimeCheck);
  preferences.putUInt("lastLogCount", _lastLogCount);

  Send_Event(countEvents, String(_count));
  // Send updated running averages after reset
  String avgData = String(_runningAverageCPM) + "," + String(_runningAverageCPH);
  Send_Event(runningAverageEvents, avgData);

#ifdef DEBUG
  Serial.println("Count reset to " + String(_count));
  Serial.println("Running averages reset.");
  Serial.println("Last logged count reset.");
#endif
}

// @note This function calculates and updates the running averages
void Update_Running_Averages()
{
  unsigned long currentTime = millis();
  // Calculate rate periodically, e.g., every 100ms or 1000ms
  const unsigned long updateInterval = 1000; // Update every 1 second

  if (currentTime - _lastTimeCheck >= updateInterval)
  {
    unsigned long deltaTime = currentTime - _lastTimeCheck;
    uint countIncrease = _count - _lastCountCheck;

    if (deltaTime > 0)
    {
      // Calculate the instantaneous rate in counts per millisecond
      double currentRatePerMs = (double)countIncrease / deltaTime;

      // Update Running Average CPM (counts per minute)
      // Convert rate per ms to rate per minute (multiply by 1000 ms/s * 60 s/min)
      double currentRatePerMinute = currentRatePerMs * 1000.0 * 60.0;
      _runningAverageCPM = (ALPHA_CPM * currentRatePerMinute) + ((1.0 - ALPHA_CPM) * _runningAverageCPM);

      // Update Running Average CPH (counts per hour)
      // Convert rate per ms to rate per hour (multiply by 1000 ms/s * 3600 s/hour)
      double currentRatePerHour = currentRatePerMs * 1000.0 * 3600.0;
      _runningAverageCPH = (ALPHA_CPH * currentRatePerHour) + ((1.0 - ALPHA_CPH) * _runningAverageCPH);

      // Ensure averages don't go below zero due to floating point inaccuracies
      if (_runningAverageCPM < 0)
        _runningAverageCPM = 0;
      if (_runningAverageCPH < 0)
        _runningAverageCPH = 0;

      // #ifdef DEBUG
      //       Serial.printf("Count Increase: %u, Delta Time: %lu ms, Rate/s: %.2f\n", countIncrease, deltaTime, currentRatePerMs * 1000.0);
      //       Serial.printf("Running Avg CPM: %.2f, Running Avg CPH: %.2f\n", _runningAverageCPM, _runningAverageCPH);
      // #endif

      // Send the updated running averages via event source
      String avgData = String(_runningAverageCPM) + "," + String(_runningAverageCPH);
      Send_Event(runningAverageEvents, avgData);
    }

    // Update last check time and count
    _lastTimeCheck = currentTime;
    _lastCountCheck = _count;
  }
}
void Switch_Init(bool activeHigh)
{
  if (activeHigh)
  {
    pinMode(SWITCH_PIN_1, INPUT_PULLDOWN);
    pinMode(SWITCH_PIN_2, INPUT_PULLDOWN);
  } else {
    pinMode(SWITCH_PIN_1, INPUT_PULLUP);
    pinMode(SWITCH_PIN_2, INPUT_PULLUP);
  }
}

// @param debounceInterval interval in millisecond
// @param activeHigh is the switch active high
void Read_Switch(ulong debounceInterval, bool activeHigh)
{
  ulong currentTime = millis();
  bool currentState = digitalRead(SWITCH_PIN_2);
  if (currentTime - _lastDebounceTime > debounceInterval)
  {
    if (currentState != _lastState)
    {
      if (currentState == (activeHigh ? HIGH : LOW))
      {
        _count++;
        // Only send total count event here. Running average is updated separately.
        Send_Event(countEvents, String(_count));
#ifdef DEBUG
        Serial.println("count: " + String(_count));
#endif
      }
      _lastState = currentState;
      _lastDebounceTime = currentTime;
    }
  }
}

void RTC_Init()
{
  if (!rtc.begin())
  {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    // while (1)
    //   ;
  }
  if (rtc.lostPower())
  {
    Serial.println("RTC lost power, setting the time!");
    // Set the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  rtc.disable32K();
}

DateTime RTC_getTime()
{
  // It's generally better to initialize RTC once in setup.
  // Calling rtc.begin() here repeatedly might cause issues.
  // Assuming RTC is initialized in setup(), we just need to get the time.
  // If rtc.begin() fails in setup, the program will hang, as intended.
  DateTime now = rtc.now();
  return now;
}

void SD_Init()
{
  if (!SD.begin())
  {
#ifdef DEBUG
    Serial.println("Card Mount Failed");
#endif
    // Consider adding a way to indicate this failure, e.g., a flag or status variable
    return;
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE)
  {
#ifdef DEBUG
    Serial.println("No SD card attached");
#endif
    // Indicate no card is present
    return;
  }
#ifdef DEBUG
  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC)
  {
    Serial.println("MMC");
  }
  else if (cardType == CARD_SD)
  {
    Serial.println("SDSC");
  }
  else if (cardType == CARD_SDHC)
  {
    Serial.println("SDHC");
  }
  else
  {
    Serial.println("Unknown type");
  }
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
#endif
}

String listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
  JsonDocument doc;

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
    String filePath = file.name();

    if (file.isDirectory())
    {
      if (levels > 0)
      {
        filePath = "/" + filePath;
        String childrenJsonString = listDir(fs, filePath.c_str(), levels - 1);
        if (childrenJsonString != "Failed to open directory" && childrenJsonString != "Not a directory")
        {
          JsonDocument childrenDoc;
          DeserializationError error = deserializeJson(childrenDoc, childrenJsonString);
          if (error)
#ifdef DEBUG
            Serial.println("Failed to parse children JSON");
#endif
          else
          {
            for (auto child : childrenDoc.as<JsonArray>())
            {
              doc.add(child);
            }
          }
        }
        else
        {
#ifdef DEBUG
          Serial.printf("Failed to list sub directory: %s\n", filePath);
#endif
        }
      }
    }
    else
    {
      filePath = String(dirname) + filePath;
      doc.add(filePath);
    }
    file.close(); // Close the file handle as soon as possible
    file = root.openNextFile();
  }
  root.close();

  String jsonOutput;
  serializeJson(doc, jsonOutput);
  return jsonOutput;
}

void createDir(fs::FS &fs, const char *path)
{
#ifdef DEBUG
  Serial.printf("Creating Dir: %s\n", path);
#endif
  if (fs.mkdir(path))
  {
#ifdef DEBUG
    Serial.println("Dir created");
#endif
  }
  else
  {
#ifdef DEBUG
    Serial.println("mkdir failed");
#endif
  }
}

void removeDir(fs::FS &fs, const char *path)
{
#ifdef DEBUG
  Serial.printf("Removing Dir: %s\n", path);
#endif
  if (fs.rmdir(path))
  {
#ifdef DEBUG
    Serial.println("Dir removed");
#endif
  }
  else
  {
#ifdef DEBUG
    Serial.println("rmdir failed");
#endif
  }
}

void readFile(fs::FS &fs, const char *path)
{
#ifdef DEBUG
  Serial.printf("Reading file: %s\n", path);
#endif

  File file = fs.open(path);
  if (!file)
  {
#ifdef DEBUG
    Serial.println("Failed to open file for reading");
#endif
    return;
  }

#ifdef DEBUG
  Serial.print("Read from file: ");
  while (file.available())
  {
    Serial.write(file.read());
  }
  Serial.println(); // Add a newline after reading
#endif
  file.close();
}

void writeFile(fs::FS &fs, const char *path, const char *message)
{
#ifdef DEBUG
  Serial.printf("Writing file: %s\n", path);
#endif

  File file = fs.open(path, FILE_WRITE);
  if (!file)
  {
#ifdef DEBUG
    Serial.println("Failed to open file for writing");
#endif
    return;
  }
  if (file.print(message))
  {
#ifdef DEBUG
    Serial.println("File written");
#endif
  }
  else
  {
#ifdef DEBUG
    Serial.println("Write failed");
#endif
  }
  file.close();
}

void appendFile(fs::FS &fs, const char *path, const char *message)
{
#ifdef DEBUG
  Serial.printf("Appending to file: %s\n", path);
#endif

  File file = fs.open(path, FILE_APPEND);
  if (!file)
  {
#ifdef DEBUG
    Serial.println("Failed to open file for appending");
#endif
    return;
  }
  if (file.print(message))
  {
#ifdef DEBUG
    Serial.println("Message appended");
#endif
  }
  else
  {
#ifdef DEBUG
    Serial.println("Append failed");
#endif
  }
  file.close();
}

void renameFile(fs::FS &fs, const char *path1, const char *path2)
{
#ifdef DEBUG
  Serial.printf("Renaming file %s to %s\n", path1, path2);
#endif
  if (fs.rename(path1, path2))
  {
#ifdef DEBUG
    Serial.println("File renamed");
#endif
  }
  else
  {
#ifdef DEBUG
    Serial.println("Rename failed");
#endif
  }
}

void deleteFile(fs::FS &fs, const char *path)
{
#ifdef DEBUG
  Serial.printf("Deleting file: %s\n", path);
#endif
  if (fs.remove(path))
  {
#ifdef DEBUG
    Serial.println("File deleted");
#endif
  }
  else
  {
#ifdef DEBUG
    Serial.println("Delete failed");
#endif
  }
}

// @param interval log interval in millisecond
// @note this function will create a new file if the file does not exist
// @note the file name will be the current date in the format YYYY-MM-DD.csv
// @note the file will be created in the root directory of the SD card
void Log_SD(ulong interval)
{
  unsigned long currentTime = millis();
  // Only attempt to log if the SD card was successfully initialized, the interval has passed,
  // AND the total count has changed since the last log.
  if (SD.cardType() != CARD_NONE && currentTime - _lastLogTime > interval && _count != _lastLogCount)
  {
    // Get current date for filename
    DateTime now = RTC_getTime();
    String filename = "/";
    filename += now.timestamp(DateTime::TIMESTAMP_DATE);
    filename += ".csv";

    File dataFile;
    bool newFile = false;

    // Check if the file exists before attempting to open
    if (!SD.exists(filename.c_str()))
    {
#ifdef DEBUG
      Serial.print("File ");
      Serial.print(filename);
      Serial.println(" does not exist, creating new file.");
#endif
      // Attempt to create and open the file for writing
      dataFile = SD.open(filename.c_str(), FILE_WRITE);
      if (dataFile)
      {
        // Add columns for running averages
        dataFile.println("time,count,cpm,cph");
        newFile = true;
      }
      else
      {
#ifdef DEBUG
        Serial.println("Error creating file: ");
        Serial.println(filename);
#endif
        // Return if file creation failed
        return;
      }
    }
    else
    {
      // Attempt to open the existing file for appending
      dataFile = SD.open(filename.c_str(), FILE_APPEND);
      if (!dataFile)
      {
#ifdef DEBUG
        Serial.println("Error opening file: ");
        Serial.println(filename);
#endif
        // Return if file opening failed
        return;
      }
    }

    // Check if the file is valid before writing
    if (dataFile)
    {
      String fullTimestamp = now.timestamp();

      // Attempt to write data
      // Prepare the log entry as a single string
      String logEntry = fullTimestamp;
      logEntry += ",";
      logEntry += String(_count); // Log total count
      logEntry += ",";
      logEntry += String(_runningAverageCPM, 2); // Log running average CPM with 2 decimal places
      logEntry += ",";
      logEntry += String(_runningAverageCPH, 2); // Log running average CPH with 2 decimal places

      // Write the consolidated log entry
      dataFile.println(logEntry);

      // Close the file regardless of perceived write success to free resources.
      dataFile.close();

#ifdef DEBUG
      if (newFile)
      {
        Serial.println("New file created and first data logged.");
      }
      else
      {
        Serial.println("Data appended to existing file.");
      }
      Serial.print("Logged to ");
      Serial.print(filename);
      Serial.print(": ");
      Serial.print(fullTimestamp);
      Serial.print(", ");
      Serial.print(_count);
      Serial.print(", ");
      Serial.print(_runningAverageCPM, 2);
      Serial.print(", ");
      Serial.println(_runningAverageCPH, 2);
#endif

      _lastLogTime = currentTime;
      _lastLogCount = _count; // Update the last logged count
    }
    else
    {
#ifdef DEBUG
      Serial.println("Error: File handle is invalid after open attempt.");
#endif
    }
  }
  else if (SD.cardType() == CARD_NONE)
  {
#ifdef DEBUG
    // Optionally print a message if SD card is not present, but not repeatedly
#endif
  }
  // If the interval has passed but the count hasn't changed, we just skip logging for this interval.
}
