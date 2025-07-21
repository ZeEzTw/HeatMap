#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Preferences.h>
#include <time.h>
#include "Error.h"
#include "LocalStorage.h"

Preferences prefs;
LocalStorage localStorage;

// Variabile configurabile, folosește String în loc de char* pentru a putea lucra ușor cu Preferences
String ssid = "hotspot";
String password = "123456789";

// InfluxDB Cloud config
String influx_host = "https://eu-central-1-1.aws.cloud2.influxdata.com";
String org = "Eli-np";
String bucket = "temperature%20and%20humidity%20data";
String token = "j60HrDCjeKlEyAc4m_GrYpFNjIpX--Jv9UX1v7qYtZxdyXfyuPwh_dqLl_bbJCDPi8hk-gJn_dksyh2eE11eug==";

// Pins for 4 sensors
int sdaPin[4] = {21, 26, 33, 19};
int sclPin[4] = {22, 27, 25, 18};
String sensorIDs[4] = {"001", "002", "003", "004"};

TwoWire I2C = TwoWire(0);

struct SensorData {
  float temperature;
  float humidity;
};

// Number of samples to collect per sensor (configurable)
int numSamples = 1;

// NTP sync
int syncSecond = 0; // configurable sync second

void loadSettings() {
  prefs.begin("config", true); // open NVS for reading

  ssid = prefs.getString("ssid", ssid);
  password = prefs.getString("password", password);

  influx_host = prefs.getString("influx_host", influx_host);
  org = prefs.getString("org", org);
  bucket = prefs.getString("bucket", bucket);
  token = prefs.getString("token", token);

  for (int i = 0; i < 4; i++) {
    String key = "sensorID" + String(i);
    sensorIDs[i] = prefs.getString(key.c_str(), sensorIDs[i]);
  }
  numSamples = prefs.getInt("numSamples", numSamples);
  syncSecond = prefs.getInt("syncSecond", syncSecond);

  prefs.end();
}

void saveSettings() {
  prefs.begin("config", false); // open NVS for writing

  prefs.putString("ssid", ssid);
  prefs.putString("password", password);

  prefs.putString("influx_host", influx_host);
  prefs.putString("org", org);
  prefs.putString("bucket", bucket);
  prefs.putString("token", token);

  for (int i = 0; i < 4; i++) {
    String key = "sensorID" + String(i);
    prefs.putString(key.c_str(), sensorIDs[i]);
  }
  prefs.putInt("numSamples", numSamples);
  prefs.putInt("syncSecond", syncSecond);

  prefs.end();
}

void setup() {
  Serial.begin(115200);
  loadSettings();  // încarcă setările din NVS sau folosește valorile default

  Serial.print("SSID: "); Serial.println(ssid);
  Serial.print("Password: "); Serial.println(password);

  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.print("Conectare la WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    blinkError(3); // blink once for connection attempt
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectat!");

  // SNTP setup
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Astept sincronizare SNTP...");
  time_t now = time(nullptr);
  while (now < 1000) { // wait for valid time
    delay(100);
    now = time(nullptr);
    Serial.print(".");
  }
  Serial.println("\nTimp sincronizat!");
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  Serial.print("Ora curenta: ");
  Serial.println(asctime(&timeinfo));
}

SensorData readSensor(int sda, int scl) {
  SensorData result = {NAN, NAN};
  Adafruit_AHTX0 aht;
  sensors_event_t humidity, temperature;

  I2C.begin(sda, scl);
  delay(50);

  if (aht.begin(&I2C)) {
    aht.getEvent(&humidity, &temperature);
    result.temperature = temperature.temperature;
    result.humidity = humidity.relative_humidity;
  } else {
    Serial.println("Eroare la initializarea senzorului!");
  }

  I2C.end();
  return result;
}

void reconnectWiFi() {
    blinkError(3); // blink 3 times to signal WiFi drop
    Serial.println("WiFi deconectat! Reincercare...");
    WiFi.disconnect();
    WiFi.begin(ssid.c_str(), password.c_str());
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi reconectat!");
  } else {
    Serial.println("Nu s-a putut reconecta WiFi!");
  }
}

void loop() {
  // Wait until the current second matches syncSecond
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  while (timeinfo.tm_sec != syncSecond) {
    delay(100);
    now = time(nullptr);
    localtime_r(&now, &timeinfo);
  }

  for (int i = 0; i < 3; i++) {
    float tempSum = 0.0;
    float humSum = 0.0;
    int validCount = 0;
    for (int j = 0; j < numSamples; j++) {
      SensorData data = readSensor(sdaPin[i], sclPin[i]);
      float temp = data.temperature;
      float hum = data.humidity;
      if (!isnan(temp) && !isnan(hum)) {
        tempSum += temp;
        humSum += hum;
        validCount++;
      }
      delay(550); // fast collection, ~500-600 ms
    }
    if (validCount == 0) {
      blinkError(1);
      Serial.println("Date senzor invalide.");
      continue;
    }
    float avgTemp = tempSum / validCount;
    float avgHum = humSum / validCount;
    Serial.print("Sensor "); Serial.print(i);
    Serial.print(" | Temp: "); Serial.print(avgTemp); Serial.print(" °C | ");
    Serial.print("Umiditate: "); Serial.print(avgHum); Serial.println(" %");
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      String url = influx_host + "/api/v2/write?org=" + org + "&bucket=" + bucket + "&precision=s";
      http.begin(url);
      http.addHeader("Authorization", "Token " + token);
      http.addHeader("Content-Type", "text/plain");
      String body = "";
      body += "temperature,device_id=" + sensorIDs[i] + " value=" + String(avgTemp, 2) + "\n";
      body += "humidity,device_id=" + sensorIDs[i] + " value=" + String(avgHum, 2);
      Serial.println("Trimis catre InfluxDB:");
      Serial.println(body);
      int status = http.POST(body);
      Serial.print("Status POST: ");
      Serial.println(status);
      http.end();
    }
    else
    {
      blinkError(3);
      reconnectWiFi();
      // Save data locally
      LocalData data;
      data.deviceID = sensorIDs[i];
      data.temperature = avgTemp;
      data.humidity = avgHum;
      data.timestamp = time(nullptr);
      localStorage.saveData(data);
      Serial.println("Date salvate local!");
    }
    delay(500);  // slight delay between sensors
  }
  // Try to send any locally saved data if WiFi is back
  if (WiFi.status() == WL_CONNECTED) {
    std::vector<LocalData> pending = localStorage.getAllData();
    for (auto& data : pending) {
      HTTPClient http;
      String url = influx_host + "/api/v2/write?org=" + org + "&bucket=" + bucket + "&precision=s";
      http.begin(url);
      http.addHeader("Authorization", "Token " + token);
      http.addHeader("Content-Type", "text/plain");
      String body = "";
      body += "temperature,device_id=" + data.deviceID + " value=" + String(data.temperature, 2) + "\n";
      body += "humidity,device_id=" + data.deviceID + " value=" + String(data.humidity, 2);
      int status = http.POST(body);
      http.end();
      if (status == 204) {
        Serial.println("Date locale trimise catre InfluxDB!");
      }
    }
    localStorage.clearData();
  }
  delay(500); // wait before next full cycle
}
