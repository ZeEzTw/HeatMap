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
int sdaPin[4] = {18, 4, 17};
int sclPin[4] = {19, 16, 5};
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

  for (int i = 0; i < 3; i++) {
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

  for (int i = 0; i < 3; i++) {
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
  delayMicroseconds(100); // minimal delay for I2C stability

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
  // Take 5 readings from each sensor as fast as possible
  float avgTemp[3] = {0}, avgHum[3] = {0};
  int validCount[3] = {0};
  for (int i = 0; i < 3; i++) {
    float tempSum = 0.0, humSum = 0.0;
    int count = 0;
    for (int j = 0; j < numSamples; j++) {
      SensorData data = readSensor(sdaPin[i], sclPin[i]);
      float temp = data.temperature;
      float hum = data.humidity;
      if (!isnan(temp) && !isnan(hum)) {
        tempSum += temp;
        humSum += hum;
        count++;
      }
      if (j < numSamples - 1) delayMicroseconds(100); // minimal delay
    }
    validCount[i] = count;
    if (count > 0) {
      avgTemp[i] = tempSum / count;
      avgHum[i] = humSum / count;
    } else {
      avgTemp[i] = NAN;
      avgHum[i] = NAN;
      blinkError(1);
      Serial.print("Date senzor invalide pentru senzorul ");
      Serial.println(i);
    }
    // Minimal delay between sensors
    if (i < 2) delayMicroseconds(100);
  }

  // Build a single body for all sensors
  String body = "";
  for (int i = 0; i < 3; i++) {
    if (!isnan(avgTemp[i]) && !isnan(avgHum[i])) {
      body += "temperature,device_id=" + sensorIDs[i] + " value=" + String(avgTemp[i], 2) + "\n";
      body += "humidity,device_id=" + sensorIDs[i] + " value=" + String(avgHum[i], 2) + "\n";
    }
  }
  if (body.length() > 0) {
    // Remove last newline
    if (body.endsWith("\n")) body.remove(body.length() - 1);
  }

  if (WiFi.status() == WL_CONNECTED && body.length() > 0) {
    HTTPClient http;
    String url = influx_host + "/api/v2/write?org=" + org + "&bucket=" + bucket + "&precision=s";
    http.begin(url);
    http.addHeader("Authorization", "Token " + token);
    http.addHeader("Content-Type", "text/plain");
    Serial.println("Trimis catre InfluxDB:");
    Serial.println(body);
    int status = http.POST(body);
    Serial.print("Status POST: ");
    Serial.println(status);
    http.end();
  } else if (body.length() > 0) {
    // Save all data locally if WiFi is not connected
    blinkError(3);
    reconnectWiFi();
    time_t now = time(nullptr);
    for (int i = 0; i < 3; i++) {
      if (!isnan(avgTemp[i]) && !isnan(avgHum[i])) {
        LocalData data;
        data.deviceID = sensorIDs[i];
        data.temperature = avgTemp[i];
        data.humidity = avgHum[i];
        data.timestamp = now;
        localStorage.saveData(data);
      }
    }
    Serial.println("Date salvate local!");
  }

  // Try to send any locally saved data if WiFi is back
  if (WiFi.status() == WL_CONNECTED) {
    std::vector<LocalData> pending = localStorage.getAllData();
    if (!pending.empty()) {
      String localBody = "";
      for (auto& data : pending) {
        localBody += "temperature,device_id=" + data.deviceID + " value=" + String(data.temperature, 2) + "\n";
        localBody += "humidity,device_id=" + data.deviceID + " value=" + String(data.humidity, 2) + "\n";
      }
      if (localBody.endsWith("\n")) localBody.remove(localBody.length() - 1);
      HTTPClient http;
      String url = influx_host + "/api/v2/write?org=" + org + "&bucket=" + bucket + "&precision=s";
      http.begin(url);
      http.addHeader("Authorization", "Token " + token);
      http.addHeader("Content-Type", "text/plain");
      int status = http.POST(localBody);
      http.end();
      if (status == 204) {
        Serial.println("Date locale trimise catre InfluxDB!");
        localStorage.clearData();
      }
    }
  }
  // Minimal delay before next cycle
  delayMicroseconds(100);
}
