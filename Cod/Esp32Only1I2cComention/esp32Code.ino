#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Preferences.h>
#include <time.h>
#include "Error.h"
#include "LocalStorage.h"
#include <DHT.h>

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

// Number of I2C sensors to use (configurable)
int numI2CSensors = 2; // Change this to use 1, 2, 3, or 4 sensors

// Pins for 4 sensors (only first numI2CSensors will be used)
int sdaPin[4] = {4, 17, 18, 21};
int sclPin[4] = {16, 5, 19, 22};

#define DHTPIN 15
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

String sensorIDs[5] = {"001", "002", "003", "004", "005"}; // 005 = DHT11

TwoWire I2C = TwoWire(0);

struct SensorData {
  float temperature;
  float humidity;
};

// Delay between sensor readings (ms)
int sensorDelayMs = 400; // increased for long wires
// Delay after all sensors are read and data is sent (ms)
int cycleDelayMs = 10000;

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

  for (int i = 0; i < 2; i++) {
    if (i < numI2CSensors) { // Only load settings for active sensors
      String key = "sensorID" + String(i);
      sensorIDs[i] = prefs.getString(key.c_str(), sensorIDs[i]);
    }
  }
  numI2CSensors = prefs.getInt("numI2CSensors", numI2CSensors); // Load number of sensors
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
    if (i < numI2CSensors) { // Only save settings for active sensors
      String key = "sensorID" + String(i);
      prefs.putString(key.c_str(), sensorIDs[i]);
    }
  }
  prefs.putInt("numI2CSensors", numI2CSensors); // Save number of sensors
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
  Serial.print("Folosesc ");
  Serial.print(numI2CSensors);
  Serial.println(" senzori I2C + 1 DHT11");

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

  dht.begin();
}

SensorData readSensor(int sda, int scl) {
  SensorData result = {NAN, NAN};
  Adafruit_AHTX0 aht;
  sensors_event_t humidity, temperature;

  I2C.begin(sda, scl);
  I2C.setClock(10000); // lowest I2C frequency: 10kHz for very long wires
  delay(10); // increased delay for I2C stability with long wires

  if (aht.begin(&I2C)) {
    delay(20); // extra delay after sensor init for long wires
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
  // Read each AHT sensor once, with delay between each
  float temp[5] = {NAN}, hum[5] = {NAN};
  
  // Only read the number of I2C sensors specified by numI2CSensors
  for (int i = 0; i < numI2CSensors; i++) {
    Serial.print("Reading sensor ");
    Serial.print(i + 1);
    Serial.print(" (ID: ");
    Serial.print(sensorIDs[i]);
    Serial.println(")");
    
    SensorData data = readSensor(sdaPin[i], sclPin[i]);
    temp[i] = data.temperature;
    hum[i] = data.humidity;
    if (isnan(temp[i]) || isnan(hum[i])) {
      blinkError(1);
      Serial.print("Date senzor invalide pentru senzorul ");
      Serial.println(i);
    } else {
      Serial.print("Temp: ");
      Serial.print(temp[i]);
      Serial.print("°C, Hum: ");
      Serial.print(hum[i]);
      Serial.println("%");
    }
    if (i < numI2CSensors - 1) delay(sensorDelayMs); // delay between sensors
  }
  // Read DHT11 after all AHTs
  delay(sensorDelayMs); // extra delay for DHT stability
  float dht_h = dht.readHumidity();
  float dht_t = dht.readTemperature();
  if (!isnan(dht_t) && !isnan(dht_h)) {
    temp[4] = dht_t;
    hum[4] = dht_h;
  } else {
    Serial.println("Date senzor invalide pentru DHT11 (005)");
  }

  // Build a single body for all sensors
  String body = "";
  // Only process the active I2C sensors
  for (int i = 0; i < numI2CSensors; i++) {
    if (!isnan(temp[i]) && !isnan(hum[i])) {
      body += "temperature,device_id=" + sensorIDs[i] + " value=" + String(temp[i], 2) + "\n";
      body += "humidity,device_id=" + sensorIDs[i] + " value=" + String(hum[i], 2) + "\n";
    }
  }
  // Add DHT11 data (sensor index 4)
  if (!isnan(temp[4]) && !isnan(hum[4])) {
    body += "temperature,device_id=" + sensorIDs[4] + " value=" + String(temp[4], 2) + "\n";
    body += "humidity,device_id=" + sensorIDs[4] + " value=" + String(hum[4], 2) + "\n";
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
    // Save only active I2C sensor data
    for (int i = 0; i < numI2CSensors; i++) {
      if (!isnan(temp[i]) && !isnan(hum[i])) {
        LocalData data;
        data.deviceID = sensorIDs[i];
        data.temperature = temp[i];
        data.humidity = hum[i];
        data.timestamp = now;
        localStorage.saveData(data);
      }
    }
    // Save DHT11 data
    if (!isnan(temp[4]) && !isnan(hum[4])) {
      LocalData data;
      data.deviceID = sensorIDs[4];
      data.temperature = temp[4];
      data.humidity = hum[4];
      data.timestamp = now;
      localStorage.saveData(data);
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
  // Wait before next cycle
  delay(cycleDelayMs);
}
