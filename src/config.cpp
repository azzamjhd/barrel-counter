#include <config.h>

DNSServer dnsServer;
AsyncWebServer server(80);
AsyncEventSource events(EVENT_SOURCE);
LiquidCrystal_I2C LCD(0x27, 16, 2);
Preferences preferences;

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
  WiFi.mode(WIFI_AP);
  #ifndef PASSWORD
  WiFi.softAP(SSID);
  #else
  WiFi.softAP(SSID, PASSWORD);
  #endif
  WiFi.softAPConfig(apIP, apIP, gateway);
  dnsServer.start(DNS_PORT, "*", apIP);

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
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  server.onNotFound(redirectToIndex);
  server.begin();
  #ifdef DEBUG
  Serial.println("Server Started");
  #endif
}
// @note this function is called in the loop() function
void Webserver_Loop() {
  dnsServer.processNextRequest();
}
// @param eventName name of the event to send
// @param eventData data to send with the event
// @note the event name should be unique for each event
void Send_Event(const String &eventName, const String &eventData) {
  // events.send(eventData.c_str(), eventName.c_str());
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
// @param showDate if true, show date in the format dd/mm/yyyy hh:mm:ss
// @return current local time in the format dd/mm/yyyy hh:mm:ss or hh:mm:ss
// @note if showDate is false, the date will not be shown
String getCurrentLocalTime(bool showDate) {
  struct tm timeinfo;
  // Get local time from the RTC
  if (getLocalTime(&timeinfo)) {
    char buf[25];
    if (showDate) {
      // Format: dd/mm/yyyy hh:mm:ss
      sprintf(buf, "%02d/%02d/%04d %02d:%02d:%02d", timeinfo.tm_mday,
              timeinfo.tm_mon + 1, timeinfo.tm_year + 1900, timeinfo.tm_hour,
              timeinfo.tm_min, timeinfo.tm_sec);
    } else {
      // Format: hh:mm:ss
      sprintf(buf, "%02d:%02d:%02d", timeinfo.tm_hour,
              timeinfo.tm_min, timeinfo.tm_sec);
    }
    return String(buf);
  } else {
    return "Failed to obtain time";
  }
}

