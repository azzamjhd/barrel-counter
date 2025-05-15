#include <config.h>

DNSServer dnsServer;
AsyncWebServer server(80);
AsyncEventSource events(EVENT_SOURCE);
LiquidCrystal_I2C LCD(0x27, 16, 2);
Preferences preferences;
RTC_DS3231 rtc;

String STA_SSID = "your_ssid";
String STA_PASSWORD = "your_password";
String AP_SSID = "Drum Counter";
String AP_PASSWORD = "12345678";
int Log_Interval = 60; // in seconds
ulong _lastLogTime = 0;
uint _lastLogCount = 0;

volatile uint _count = 0;
ulong _lastSaveTime = 0;
ulong _lastDebounceTime = 0;
bool _lastState = 0;
DateTime _currentDate;
DateTime _lastDate;

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
#ifndef PASSWORD
  WiFi.softAP(AP_SSID.c_str());
#else
  WiFi.softAP(AP_SSID.c_str(), AP_PASSWORD.c_str());
// WiFi.begin(STA_SSID.c_str(), STA_PASSWORD.c_str());
#endif
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

String WiFi_Scan()
{
  int n = WiFi.scanNetworks();
  JsonDocument doc;
  JsonArray array = doc.to<JsonArray>();
  for (int i = 0; i < n; ++i)
  {
    JsonObject obj = array.createNestedObject();
    obj["SSID"] = WiFi.SSID(i);
    obj["RSSI"] = WiFi.RSSI(i);
    obj["Channel"] = WiFi.channel(i);
    obj["Encryption"] = WiFi.encryptionType(i);
  }
  String jsonString;
  serializeJson(doc, jsonString);
#ifdef DEBUG
  Serial.println("WiFi Scan Results: " + jsonString);
#endif
  return jsonString;
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
  server.addHandler(&events);
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
  server.on("/updateTime", HTTP_POST,
            // No response here; we wait until the body is received.
            [](AsyncWebServerRequest *request) {},
            // onUpload handler (not used here)
            NULL,
            // onBody: called as the request body is received
            [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
            {
      // Use a StaticJsonDocument; adjust capacity as needed
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, data, len);
      if (!error) {
        long timeValue = doc["time"];
        Serial.println("Time: " + String(timeValue));
        rtc.adjust(DateTime(timeValue));
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
  server.on("/wifiScan", HTTP_GET, [](AsyncWebServerRequest *request)
            {
      String jsonString = WiFi_Scan();
      request->send(200, "application/json", jsonString); });
  server.on("/deleteFile", HTTP_GET, [](AsyncWebServerRequest *request)
            {
      if (request->hasParam("file")) {
        String fileName = request->getParam("file")->value();
        if (LittleFS.remove(fileName)) request->send(200, "text/plain", "File deleted");
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
}
// @note this function is called in the loop() function
void Webserver_Loop()
{
  // dnsServer.processNextRequest();
}
// @param eventName name of the event to send
// @param eventData data to send with the event
// @note the event name should be unique for each event
void Send_Event(const String &eventName, const String &eventData)
{
  events.send(eventData.c_str());
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
}
// @param interval save interval to preferences in millisecond
void Save_To_Preferences(ulong interval)
{
  unsigned long currentTime = millis();
  if (currentTime - _lastSaveTime > interval)
  {
    preferences.putUInt(PREFERENCES_KEY_NAME, _count);
    preferences.putLong("lastDate", _currentDate.unixtime());
    _lastSaveTime = currentTime;
    ESP_LOGI("BARREL_COUNTER", "Saved _count: %d", _count);
  }
}
// @note reset _count and preferences count
void Reset_Count()
{
  _count = 0;
  preferences.putUInt(PREFERENCES_KEY_NAME, _count);
  Send_Event("counter", String(_count));
#ifdef DEBUG
  Serial.println("Count reset to " + String(_count));
#endif
}
// @param debounceInterval interval in millisecond
// @param activeHigh is the switch active high
void Read_Switch(ulong debounceInterval, bool activeHigh)
{
  ulong currentTime = millis();
  bool currentState = digitalRead(SWITCH_PIN);
  if (currentTime - _lastDebounceTime > debounceInterval)
  {
    if (currentState != _lastState)
    {
      if (currentState == activeHigh ? HIGH : LOW)
      {
        _count++;
        Send_Event("counter", String(_count));
#ifdef DEBUG
        Serial.println("Button pressed, count: " + String(_count));
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
    while (1)
      ;
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
  if (!rtc.begin())
  {
    Serial.println("Couldn't find RTC");
    return DateTime(0, 0, 0, 0, 0, 0);
  }
  DateTime now = rtc.now();
  return now;
}

void SD_Init()
{
  if (!SD.begin())
  {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE)
  {
    Serial.println("No SD card attached");
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
    return "Failed to open directory";
  }
  if (!root.isDirectory())
  {
    root.close(); // Close the handle
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
            Serial.println("Failed to parse children JSON");
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
          Serial.printf("Failed to list sub directory: %s\n", filePath);
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
  Serial.printf("Creating Dir: %s\n", path);
  if (fs.mkdir(path))
  {
    Serial.println("Dir created");
  }
  else
  {
    Serial.println("mkdir failed");
  }
}

void removeDir(fs::FS &fs, const char *path)
{
  Serial.printf("Removing Dir: %s\n", path);
  if (fs.rmdir(path))
  {
    Serial.println("Dir removed");
  }
  else
  {
    Serial.println("rmdir failed");
  }
}

void readFile(fs::FS &fs, const char *path)
{
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file)
  {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while (file.available())
  {
    Serial.write(file.read());
  }
  file.close();
}

void writeFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message))
  {
    Serial.println("File written");
  }
  else
  {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file)
  {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message))
  {
    Serial.println("Message appended");
  }
  else
  {
    Serial.println("Append failed");
  }
  file.close();
}

void renameFile(fs::FS &fs, const char *path1, const char *path2)
{
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2))
  {
    Serial.println("File renamed");
  }
  else
  {
    Serial.println("Rename failed");
  }
}

void deleteFile(fs::FS &fs, const char *path)
{
  Serial.printf("Deleting file: %s\n", path);
  if (fs.remove(path))
  {
    Serial.println("File deleted");
  }
  else
  {
    Serial.println("Delete failed");
  }
}

// @param interval log interval in millisecond
// @note this function will create a new file if the file does not exist
// @note the file name will be the current date in the format YYYY-MM-DD.csv
// @note the file will be created in the root directory of the SD card
void Log_SD(ulong interval)
{
  unsigned long currentTime = millis();
  if (currentTime - _lastLogTime > interval)
  {
    if (_count == _lastLogCount)
      return;
    _lastLogCount = _count;

    String filename = "/";
    filename += _currentDate.timestamp(DateTime::TIMESTAMP_DATE);
    filename += ".csv";

    File dataFile;
    bool newFile = false;
    if (!SD.exists(filename.c_str()))
    {
      Serial.print("File ");
      Serial.print(filename);
      Serial.println(" does not exist, creating new file.");
      dataFile = SD.open(filename.c_str(), FILE_WRITE);
      if (dataFile)
      {
        dataFile.println("time,count");
        newFile = true;
      }
      else
      {
        Serial.println("Error creating file: ");
        Serial.println(filename);
        return;
      }
    }
    else
    {
      dataFile = SD.open(filename.c_str(), FILE_APPEND);
      if (!dataFile)
      {
        Serial.println("Error opening file: ");
        Serial.println(filename);
        return;
      }
    }

    if (dataFile)
    {
      String fullTimestamp = _currentDate.timestamp();

      dataFile.print(fullTimestamp);
      dataFile.print(",");
      dataFile.println(_count);
      dataFile.close();

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
      Serial.println(_count);

      _lastLogTime = currentTime;

      return;
    }

    Serial.println("Error: File was not open for writing.");
  }
}