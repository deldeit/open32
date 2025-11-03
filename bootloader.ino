#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  4
#define TFT_SCK  18
#define TFT_MOSI 23

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

Preferences prefs;
WebServer server(80);

String apSSID = "Open32";
String apPassword;

String generatePassword() {
  String pwd = "";
  for (int i = 0; i < 8; i++) pwd += String(random(0, 10));
  return pwd;
}

String pageForm(bool wrong = false) {
  String page = "<!doctype html><html><head><meta charset='utf-8'><title>Open32</title>";
  page += "<style>body{font-family:sans-serif;text-align:center;margin-top:80px;}input{padding:8px;margin:5px;width:200px;}button{padding:10px 20px;background:#007aff;color:white;border:none;border-radius:5px;}</style></head><body>";
  page += "<h2>Connect ESP32 to Wi-Fi</h2>";
  if (wrong) page += "<p style='color:red;'>Connection failed. Please try again.</p>";
  page += "<form method='POST' action='/save'>";
  page += "<input name='ssid' placeholder='Wi-Fi Name (SSID)' required><br>";
  page += "<input name='pass' type='password' placeholder='Password'><br>";
  page += "<button type='submit'>Save and Connect</button>";
  page += "</form></body></html>";
  return page;
}

void handleRoot() {
  server.send(200, "text/html", pageForm());
}

void handleSave() {
  if (server.hasArg("ssid")) {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");

    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.end();

    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(5, 40);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);
    tft.println("Connecting to:");
    tft.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(500);
    WiFi.begin(ssid.c_str(), pass.c_str());

    unsigned long start = millis();
    bool connected = false;
    while (millis() - start < 15000) {
      if (WiFi.status() == WL_CONNECTED) {
        connected = true;
        break;
      }
      delay(200);
    }

    if (connected) {
      server.send(200, "text/html", "<html><body><h3>Connected successfully.</h3></body></html>");
      tft.fillScreen(ST77XX_BLACK);
      tft.setCursor(5, 40);
      tft.println("Connected!");
      tft.print("IP: ");
      tft.println(WiFi.localIP());
      WiFi.softAPdisconnect(true);
      server.stop();
      Serial.println("Connected to Wi-Fi.");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
    } else {
      server.send(200, "text/html", pageForm(true));
      tft.fillScreen(ST77XX_BLACK);
      tft.setCursor(5, 40);
      tft.println("Connection failed");
      tft.println("Try again from browser");
    }
  }
}

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0));

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);

  prefs.begin("wifi", true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  prefs.end();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  unsigned long start = millis();
  bool connected = false;
  while (millis() - start < 10000) {
    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      break;
    }
    delay(200);
  }

  if (connected) {
    Serial.println("Connected to saved Wi-Fi.");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    tft.setCursor(5, 40);
    tft.println("Connected to saved Wi-Fi");
    tft.print("IP: ");
    tft.println(WiFi.localIP());
  } else {
    WiFi.mode(WIFI_AP);
    apPassword = generatePassword();
    WiFi.softAP(apSSID.c_str(), apPassword.c_str());

    Serial.println("Wi-Fi setup mode.");
    Serial.print("AP: ");
    Serial.println(apSSID);
    Serial.print("Password: ");
    Serial.println(apPassword);
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());

    tft.setCursor(5, 20);
    tft.println("Wi-Fi setup mode");
    tft.print("SSID: ");
    tft.println(apSSID);
    tft.print("Pass: ");
    tft.println(apPassword);
    tft.println();
    tft.println("1. Connect to Wi-Fi above");
    tft.println("2. Open browser");
    tft.print("3. Visit ");
    tft.println(WiFi.softAPIP());

    server.on("/", handleRoot);
    server.on("/save", handleSave);
    server.begin();
  }
}

void loop() {
  server.handleClient();
}