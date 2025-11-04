#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  4

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);
WebServer server(80);
Preferences prefs;

String apPassword, ssid, pass;
String currentVersion = "0.0";

String randomPassword(int len) {
  String s = "";
  for (int i = 0; i < len; i++) s += String(random(0, 10));
  return s;
}

void showRootPage(String message = "") {
  String page = "<html><head><title>Open32</title></head><body>";
  page += "<h1>Open32 WiFi Setup</h1>";
  if (message != "") page += "<p style='color:red;'>" + message + "</p>";
  page += "<form action='/save' method='POST'>";
  page += "SSID:<br><input name='ssid'><br>";
  page += "Password:<br><input name='pass' type='password'><br><br>";
  page += "<input type='submit' value='Connect'>";
  page += "</form></body></html>";
  server.send(200, "text/html", page);
}

bool connectToWiFi(String s, String p, int timeoutSec = 10) {
  WiFi.begin(s.c_str(), p.c_str());
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutSec * 1000) delay(500);
  return WiFi.status() == WL_CONNECTED;
}

void performOTA() {
  HTTPClient httpVersion;
  httpVersion.begin("https://raw.githubusercontent.com/deldeit/Open32/main/version.txt");
  int code = httpVersion.GET();
  if (code == HTTP_CODE_OK) {
    String version = httpVersion.getString();
    version.trim();
    if (version != currentVersion) {
      tft.fillScreen(ST77XX_BLACK);
      tft.setCursor(10, 10);
      tft.println("Updating OTA...");
      HTTPClient httpBin;
      httpBin.begin("https://raw.githubusercontent.com/deldeit/Open32/main/firmware.bin");
      int codeBin = httpBin.GET();
      if (codeBin == HTTP_CODE_OK && Update.begin()) {
        Update.writeStream(*httpBin.getStreamPtr());
        if (Update.end(true)) {
          tft.println("Update complete");
          delay(2000);
          ESP.restart();
        } else {
          tft.println("Update failed");
        }
      }
      httpBin.end();
    } else {
      tft.println("Already up-to-date");
    }
  } else {
    tft.println("Version check failed");
  }
  httpVersion.end();
}

void handleSave() {
  ssid = server.arg("ssid");
  pass = server.arg("pass");
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();

  if (connectToWiFi(ssid, pass)) {
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(10, 10);
    tft.println("Connected!");
    tft.print("IP: "); tft.println(WiFi.localIP());
    server.send(200, "text/html", "<html><body><h1>Connected! OTA starting...</h1></body></html>");
    performOTA();
  } else {
    showRootPage("Connection failed. Try again.");
  }
}

void setup() {
  randomSeed(analogRead(0));
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);

  apPassword = randomPassword(8);
  WiFi.softAP("Open32", apPassword.c_str());
  IPAddress IP = WiFi.softAPIP();
  tft.setCursor(10, 10);
  tft.println("AP: Open32");
  tft.print("Pass: "); tft.println(apPassword);
  tft.print("IP: "); tft.println(IP);

  prefs.begin("wifi", false);
  ssid = prefs.getString("ssid", "");
  pass = prefs.getString("pass", "");
  prefs.end();

  if (ssid != "") {
    if (connectToWiFi(ssid, pass)) {
      tft.fillScreen(ST77XX_BLACK);
      tft.setCursor(10, 10);
      tft.println("Connected to saved WiFi");
      tft.print("IP: "); tft.println(WiFi.localIP());
      performOTA();
    } else {
      prefs.begin("wifi", false);
      prefs.clear();
      prefs.end();
      tft.println("Failed to connect. AP active for config.");
    }
  }

  server.on("/", [](){ showRootPage(); });
  server.on("/save", handleSave);
  server.begin();
}

void loop() {
  server.handleClient();
}