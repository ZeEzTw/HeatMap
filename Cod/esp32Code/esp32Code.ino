#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "Error.h"
#include "readConfig.h"
#include "time.h"
#include "MQTT.h"

Adafruit_AHTX0 aht;

ConfigManager config;

String deviceID;
String ssid;
String password;
String influx_url;
String influx_token;
String influx_org;
String influx_bucket;
String mqtt_broker_ip;
int mqtt_broker_port = 1883;
int delaySeconds = 10;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  config.begin();
  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.print("🔌 Conectare la WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi conectat!");

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

  //mqttClient.setServer(mqtt_broker_ip.c_str(), mqtt_broker_port);
  //mqttClient.setCallback(callback);
}

void loop() {
  //if (!mqttClient.connected()) reconnectMQTT();
  //mqttClient.loop();

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

    String body = "";
    body += "temperature,device_id=" + deviceID + " value=" + String(temp, 2) + "\n";
    body += "humidity,device_id=" + deviceID + " value=" + String(hum, 2) + "\n";
    body += "esp_ip,device_id=" + deviceID + " value=\"" + WiFi.localIP().toString() + "\"";

    Serial.println("📤 Trimit către InfluxDB:");
    Serial.println(body);

    int status = http.POST(body);
    Serial.print("📬 Status POST: ");
    Serial.println(status);
    http.end();
  }

  if (blinking) {
    static unsigned long blinkEndTime = 0;
    if (blinkEndTime == 0) {
      blinkEndTime = millis() + 5000;  // Blink timp de 5 secunde
    }
    blinkLED();
    if (millis() > blinkEndTime) {
      blinking = false;
      digitalWrite(LED_PIN, LOW);
      blinkEndTime = 0;
      Serial.println("🔆 Blink oprit");
    }
  }

  String payload = "{\"temp\": " + String(temp, 2) + ", \"hum\": " + String(hum, 2) + "}";
  //mqttClient.publish(("sensors/" + deviceID).c_str(), payload.c_str());

  delay(200);
}
