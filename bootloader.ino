#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  4

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
Preferences preferences;
WebServer server(80);

String ssid, password;
String localVersion = "0.0";

const char* versionURL = "https://raw.githubusercontent.com/deldeit/Open32/main/version.txt";
const char* firmwareURL = "https://raw.githubusercontent.com/deldeit/Open32/main/firmware.bin";

void showMessage(const String &msg) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.println(msg);
}

void handleRoot() {
  String page = "<html><body style='font-family:sans-serif;text-align:center;'>";
  page += "<h2>Wi-Fi Setup</h2>";
  page += "<form action='/save' method='POST'>";
  page += "SSID:<br><input name='ssid'><br>Password:<br><input name='pass' type='password'><br><br>";
  page += "<input type='submit' value='Save & Connect'>";
  page += "</form></body></html>";
  server.send(200, "text/html", page);
}

void handleSave() {
  ssid = server.arg("ssid");
  password = server.arg("pass");
  preferences.begin("wifi", false);
  preferences.putString("ssid", ssid);
  preferences.putString("pass", password);
  preferences.end();
  server.send(200, "text/html", "<html><body><h3>Credentials saved. Rebooting...</h3></body></html>");
  delay(2000);
  ESP.restart();
}

void startAP() {
  showMessage("Wi-Fi not set.\nCreating hotspot...");
  WiFi.softAP("ESP32_Config");
  IPAddress IP = WiFi.softAPIP();
  showMessage("AP: ESP32_Config\nGo to: http://" + IP.toString());
  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.begin();
  while (true) {
    server.handleClient();
    delay(10);
  }
}

String getRemoteVersion() {
  HTTPClient http;
  http.begin(versionURL);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String version = http.getString();
    version.trim();
    http.end();
    return version;
  }
  http.end();
  return "";
}

bool downloadFirmware() {
  HTTPClient http;
  http.begin(firmwareURL);
  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }
  int len = http.getSize();
  WiFiClient* stream = http.getStreamPtr();
  if (!Update.begin(len)) {
    http.end();
    return false;
  }
  size_t written = Update.writeStream(*stream);
  if (written == len && Update.end(true)) {
    http.end();
    return true;
  }
  http.end();
  return false;
}

void checkForOTA() {
  preferences.begin("system", false);
  localVersion = preferences.getString("version", "0.0");
  preferences.end();
  showMessage("Checking for updates...");
  String remoteVersion = getRemoteVersion();
  if (remoteVersion == "") {
    showMessage("Version check failed.");
    return;
  }
  if (remoteVersion != localVersion) {
    showMessage("New version: " + remoteVersion + "\nUpdating...");
    if (downloadFirmware()) {
      preferences.begin("system", false);
      preferences.putString("version", remoteVersion);
      preferences.end();
      showMessage("Update done.\nRebooting...");
      delay(2000);
      ESP.restart();
    } else {
      showMessage("Update failed.");
    }
  } else {
    showMessage("Up to date.\nVersion: " + localVersion);
  }
}

void connectWiFi() {
  showMessage("Connecting to Wi-Fi...");
  WiFi.begin(ssid.c_str(), password.c_str());
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    showMessage("Connected!\nIP: " + WiFi.localIP().toString());
    delay(1000);
    checkForOTA();
  } else {
    showMessage("Connection failed.\nStarting AP...");
    delay(1500);
    startAP();
  }
}

void setup() {
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(true);
  showMessage("Booting...");
  delay(500);
  preferences.begin("wifi", true);
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("pass", "");
  preferences.end();
  if (ssid == "" || password == "") {
    startAP();
  } else {
    connectWiFi();
  }
}

void loop() {
}