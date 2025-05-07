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

volatile uint _count = 0;
ulong _lastSaveTime = 0;
ulong _lastDebounceTime = 0;
bool _lastState = 0;

void redirectToIndex(AsyncWebServerRequest *request) {
  request->redirect("http://" + apIP.toString());
}
// @note this function is called in the setup() function
void Webserver_Init() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  // WiFi.mode(WIFI_AP_STA);
  WiFi.mode(WIFI_AP);
  #ifndef PASSWORD
  WiFi.softAP(SSID);
  #else
  WiFi.softAP(AP_SSID.c_str(), AP_PASSWORD.c_str());
  // WiFi.begin(STA_SSID.c_str(), STA_PASSWORD.c_str());
  #endif
  WiFi.softAPConfig(apIP, apIP, gateway);
  // dnsServer.start(DNS_PORT, "*", apIP);

  #ifdef DEBUG
  Serial.println("\nWiFi AP is now running\nIP address: ");
  Serial.println(WiFi.softAPIP());
  #endif

  if (!LittleFS.begin()) {
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
  server.serveStatic("/data", SD, "/").setCacheControl("max-age=600");
  // server.onNotFound(redirectToIndex);
  server.begin();
  #ifdef DEBUG
  Serial.println("Server Started");
  #endif
}

void Webserver_Routes() {
  server.on("/updateTime", HTTP_POST, 
    // No response here; we wait until the body is received.
    [](AsyncWebServerRequest *request) {},
    // onUpload handler (not used here)
    NULL,
    // onBody: called as the request body is received
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
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
      }
    }
  );
  server.on("/getCount", HTTP_GET, [](AsyncWebServerRequest *request) {
      String count = String(_count);
      request->send(200, "text/plain", count);
    });
}
// @note this function is called in the loop() function
void Webserver_Loop() {
  // dnsServer.processNextRequest();
}
// @param eventName name of the event to send
// @param eventData data to send with the event
// @note the event name should be unique for each event
void Send_Event(const String &eventName, const String &eventData) {
  events.send(eventData.c_str());
}

void LCD_Init() {
  LCD.init();
  LCD.backlight();
}

void Preferences_Init() 
{
  preferences.begin("barrel");
  _count = preferences.getUInt(PREFERENCES_KEY_NAME, 0);
}
// @param interval save interval to preferences in millisecond
void Save_To_Preferences(ulong interval) {
  unsigned long currentTime = millis();
  if (currentTime - _lastSaveTime > interval) {
    preferences.putUInt(PREFERENCES_KEY_NAME, _count);
    _lastSaveTime = currentTime;
    ESP_LOGI("BARREL_COUNTER", "Saved _count: %d", _count);
  }  
}
// @param debounceInterval interval in millisecond
// @param activeHigh is the switch active high
void Read_Switch(ulong debounceInterval, bool activeHigh) {
  ulong currentTime = millis();
  bool currentState = digitalRead(SWITCH_PIN);
  if (currentTime - _lastDebounceTime > debounceInterval) {
    if (currentState != _lastState) {
      if (currentState == activeHigh ? HIGH : LOW) {
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

void RTC_Init() {
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1);
  }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting the time!");
    // Set the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

DateTime RTC_getTime() {
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    return DateTime(0, 0, 0, 0, 0, 0);
  }
  DateTime now = rtc.now();
  return now;
}

void SD_Init() {
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
  #ifdef DEBUG
  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("Unknown type");
  }
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
  #endif
}