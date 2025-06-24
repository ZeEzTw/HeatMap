#ifndef READ_CONFIG_H
#define READ_CONFIG_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Variabile externe care vor fi completate de funcție
extern String deviceID;
extern String ssid;
extern String password;
extern String influx_url;
extern String influx_token;
extern String influx_org;
extern String influx_bucket;
extern int delaySeconds;

inline void readConfig() {
  if (!LittleFS.begin()) {
    Serial.println("⚠️ Eroare la montarea LittleFS!");
    return;
  }

  File file = LittleFS.open("/config.json", "r");
  if (!file) {
    Serial.println("⚠️ Nu am găsit config.json!");
    return;
  }

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, file);
  if (err) {
    Serial.print("⚠️ Eroare la citire JSON: ");
    Serial.println(err.c_str());
    file.close();
    return;
  }

  deviceID      = doc["device_id"] | "default";
  ssid          = doc["wi-fi name"] | "";
  password      = doc["wi-fi password"] | "";
  influx_url    = doc["influxdb"]["url"] | "";
  influx_token  = doc["influxdb"]["token"] | "";
  influx_org    = doc["influxdb"]["org"] | "";
  influx_bucket = doc["influxdb"]["bucket"] | "";
  delaySeconds  = doc["timeBetweenReadings"] | 10;

  Serial.println("📄 Configurație citită:");
  Serial.println("  device_id: " + deviceID);
  Serial.println("  WiFi: " + ssid);
  Serial.println("  InfluxDB URL: " + influx_url);
  
  file.close();
}

#endif
