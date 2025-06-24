// main.ino
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "Error.h"
#include "readConfig.h"
#include "time.h"

Adafruit_AHTX0 aht;

// Config din JSON
String deviceID;
String ssid;
String password;
String influx_url;
String influx_token;
String influx_org;
String influx_bucket;
int delaySeconds = 10;

// NTP config
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  readConfig();

  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.print("🔌 Conectare la WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi conectat!");

  // Setup NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("⏰ Aștept sincronizare timp NTP...");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\n✅ Timp sincronizat!");

  Wire.begin(21, 22);
  if (!aht.begin()) {
    Serial.println("❌ Eroare: senzor AHT21 nu detectat!");
    blinkError(3, 3);
    while (true) delay(10);
  }
  Serial.println("✅ Senzor AHT21 OK");
}

void loop() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("⚠️ Nu am timp, încerc din nou...");
    delay(1000);
    return;
  }

  int sec = timeinfo.tm_sec;
  if (sec % delaySeconds != 0) {
    delay(200);
    return;
  }

  static int lastSentSec = -1;
  if (lastSentSec == sec) {
    delay(200);
    return;
  }
  lastSentSec = sec;

  sensors_event_t humidity, temperature;
  aht.getEvent(&humidity, &temperature);

  float temp = temperature.temperature;
  float hum = humidity.relative_humidity;

  if (isnan(temp) || isnan(hum)) {
    Serial.println("⚠️ Date senzor invalide.");
    blinkError(2, 3);
    delay(delaySeconds * 1000);
    return;
  }

  Serial.printf("📊 Temp: %.2f °C | Umiditate: %.2f %%\n", temp, hum);

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String url = influx_url + "/api/v2/write?org=" + influx_org + "&bucket=" + influx_bucket + "&precision=s";
    http.begin(url);
    http.addHeader("Authorization", "Token " + influx_token);
    http.addHeader("Content-Type", "text/plain");

    String body = "temperature,device_id=" + deviceID + " value=" + String(temp, 2) + "\n";
    body += "humidity,device_id=" + deviceID + " value=" + String(hum, 2);

    Serial.println("📤 Trimit către InfluxDB:");
    Serial.println(body);

    int status = http.POST(body);
    Serial.print("📬 Status POST: ");
    Serial.println(status);
    http.end();
  }

  delay(200);
}
