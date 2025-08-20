#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Preferences.h>
#include <time.h>
#include "Error.h"
#include "ConfigData.h"
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <WiFiClientSecure.h>

struct SensorData {
  float temperature;
  float humidity;
};

#define WDT_TIMEOUT 100 // seconds
int sensorDelayMs = 400; // increased for long wires
int cycleDelayMs = 5000;

using namespace std;
String ssid = WIFI_SSID;
String password = WIFI_PASSWORD;
String influx_host = INFLUX_HOST;
String org = INFLUX_ORG;
String bucket = INFLUX_BUCKET;
String token = INFLUX_TOKEN;
int numI2CSensors = NUM_I2C_SENSORS;
int sdaPin[4] = {SDA_PINS[0], SDA_PINS[1], SDA_PINS[2], SDA_PINS[3]};
int sclPin[4] = {SCL_PINS[0], SCL_PINS[1], SCL_PINS[2], SCL_PINS[3]};
String sensorIDs[5] = {SENSOR_IDS[0], SENSOR_IDS[1], SENSOR_IDS[2], SENSOR_IDS[3]};

TwoWire I2C = TwoWire(0);

// I2C bus recovery: pulse SCL if SDA is stuck LOW, then generate STOP, reinit bus
bool i2cBusRecovery(int scl, int sda) {
  pinMode(scl, OUTPUT);
  pinMode(sda, INPUT_PULLUP);
  // Pulse SCL 20 times if SDA is LOW
  for (int i = 0; i < 20 && digitalRead(sda) == LOW; i++) {
    digitalWrite(scl, HIGH);
    delayMicroseconds(5);
    digitalWrite(scl, LOW);
    delayMicroseconds(5);
  }
  // Generate STOP: SDA LOW->HIGH with SCL HIGH
  pinMode(sda, OUTPUT);
  digitalWrite(sda, LOW);
  delayMicroseconds(5);
  digitalWrite(scl, HIGH);
  delayMicroseconds(5);
  digitalWrite(sda, HIGH);
  delayMicroseconds(5);
  // Reinit I2C
  Wire.end();
  delay(10);
  Wire.begin(sda, scl);
  delay(10);
  pinMode(sda, INPUT_PULLUP);
  pinMode(scl, INPUT_PULLUP);
  return digitalRead(sda) == HIGH;
}

// Restart ESP (last resort)
void restartESP() {
  Serial.println("Restart ESP...");
  delay(1000);
  ESP.restart();
}

// Read sensor with I2C recovery and retry
SensorData readSensorWithRecovery(int sda, int scl, int maxTries = 2) {
  SensorData data = readSensor(sda, scl);
  int tries = 1;
  while ((isnan(data.temperature) || isnan(data.humidity)) && tries < maxTries) {
    Serial.println("I2C error, attempting bus recovery...");
    if (i2cBusRecovery(scl, sda)) {
      data = readSensor(sda, scl);
    } else {
      Serial.println("I2C bus recovery failed!");
      break;
    }
    tries++;
  }
  return data;
}

// Monitoring/logging function
void logSystemStatus(unsigned long i2cErrorCount) {
  Serial.print("[MONITOR] millis: ");
  Serial.print(millis());
  Serial.print(", freeHeap: ");
  Serial.print(ESP.getFreeHeap());
  Serial.print(", WiFi RSSI: ");
  Serial.print(WiFi.RSSI());
  Serial.print(", reset reason: ");
  Serial.print(esp_reset_reason());
  Serial.print(", i2cErrorCount: ");
  Serial.println(i2cErrorCount);

  // Log system status, including i2cErrorCount, to InfluxDB
  String debugBody = "debug,device_id=ESP32 reset_reason=" + String(esp_reset_reason()) + ",wifi_rssi=" + String(WiFi.RSSI()) + ",i2c_error_count=" + String(i2cErrorCount) + ",reset_status=1";
  HTTPClient debugHttp;
  debugHttp.begin(influx_host + "/api/v2/write?org=" + org + "&bucket=" + bucket + "&precision=s");
  debugHttp.addHeader("Authorization", "Token " + token);
  debugHttp.addHeader("Content-Type", "text/plain");
  debugHttp.POST(debugBody);
  debugHttp.end();
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Configuring WDT...");
  Serial.print("Watchdog Timeout (in seconds) set to: ");
  Serial.println(WDT_TIMEOUT);

  // Dezactivează orice WDT vechi (ca să nu fie conflict)
  esp_task_wdt_deinit();

  // Configurare WDT (pentru toate core-urile CPU)
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WDT_TIMEOUT * 1000,                 // timeout în ms
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,  // toate core-urile
    .trigger_panic = true                             // panic -> restart
  };

  // Inițializează WDT
  esp_err_t wdt_init_result = esp_task_wdt_init(&wdt_config);
  if (wdt_init_result != ESP_OK) {
    Serial.println("Failed to initialize watchdog timer");
    // Handle the error (e.g., restart or log)
  }

  // Add current thread to the watchdog timer with error handling
  esp_err_t wdt_add_result = esp_task_wdt_add(NULL); // Store the result of adding the task
  if (wdt_add_result != ESP_OK) {
    Serial.println("Failed to add task to watchdog timer");
    // Handle the error (e.g., restart or log)
  }


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

void writeToInfluxDB(const String& body) {
  static int influxFailedAttempts = 0; // Track consecutive failed attempts for InfluxDB

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

    if (status == 204) { // Success
      influxFailedAttempts = 0; // Reset failed attempts counter for InfluxDB
    } else {
      influxFailedAttempts++;
      Serial.println("Failed to send data to InfluxDB");
    }

    http.end();
  } else if (body.length() > 0) {
    blinkError(3);
    reconnectWiFi();
    influxFailedAttempts++;
  }

  // Reset the board after 5 consecutive failed attempts for InfluxDB
  if (influxFailedAttempts >= 5) {
    Serial.println("5 consecutive failed attempts to send data to InfluxDB. Restarting ESP...");
    restartESP();
  }

  // Log debug information to InfluxDB
  String debugBody = "debug,device_id=ESP32 influx_error_count=" + String(influxFailedAttempts) + ",wifi_rssi=" + String(WiFi.RSSI());
  HTTPClient debugHttp;
  debugHttp.begin(influx_host + "/api/v2/write?org=" + org + "&bucket=" + bucket + "&precision=s");
  debugHttp.addHeader("Authorization", "Token " + token);
  debugHttp.addHeader("Content-Type", "text/plain");
  debugHttp.POST(debugBody);
  debugHttp.end();
}

void loop() {
  static unsigned long i2cErrorCount = 0;
  static unsigned long lastLog = 0;
  static int sensorFailedAttempts = 0; // Track consecutive failed attempts for sensors

  // Reset the watchdog timer at the start of the loop
  esp_task_wdt_reset();
  delay(10);
  float temp[5] = {NAN}, hum[5] = {NAN};
  int failedSensors = 0;
  // Only read the number of I2C sensors specified by numI2CSensors
  for (int i = 0; i < numI2CSensors; i++) {
    Serial.print("Reading sensor ");
    Serial.print(i + 1);
    Serial.print(" (ID: ");
    Serial.print(sensorIDs[i]);
    Serial.println(")");
    SensorData data = readSensorWithRecovery(sdaPin[i], sclPin[i]);
    temp[i] = data.temperature;
    hum[i] = data.humidity;
    if (isnan(temp[i]) || isnan(hum[i])) {
      blinkError(1);
      Serial.print("Date senzor invalide pentru senzorul ");
      Serial.println(i);
      i2cErrorCount++;
      failedSensors++;
    } else {
      Serial.print("Temp: ");
      Serial.print(temp[i]);
      Serial.print("°C, Hum: ");
      Serial.print(hum[i]);
      Serial.println("%");
    }
    if (i < numI2CSensors - 1) delay(sensorDelayMs);
  }
  // Reset the watchdog timer after sensor readings
  esp_task_wdt_reset();

  // If all sensors failed for 3 consecutive cycles, increment sensorFailedAttempts
  if (failedSensors == numI2CSensors) {
    sensorFailedAttempts++;
    if (sensorFailedAttempts >= 3) {
      Serial.println("All sensors failed 3 times, restarting ESP...");
      restartESP();
    }
  } else {
    sensorFailedAttempts = 0;
  }

  // Build a single body for all sensors
  String body = "";
  for (int i = 0; i < numI2CSensors; i++) {
    if (!isnan(temp[i]) && !isnan(hum[i])) {
      body += "temperature,device_id=" + sensorIDs[i] + " value=" + String(temp[i], 2) + "\n";
      body += "humidity,device_id=" + sensorIDs[i] + " value=" + String(hum[i], 2) + "\n";
    }
  }
  if (body.length() > 0) {
    if (body.endsWith("\n")) body.remove(body.length() - 1);
  }
  writeToInfluxDB(body);
  // Reset the watchdog timer after sending data to InfluxDB
  esp_task_wdt_reset();

  // Periodic monitoring/logging
  if (millis() - lastLog > 60000) { // every 60s
    logSystemStatus(i2cErrorCount);
    lastLog = millis();
  }
  delay(cycleDelayMs);
  esp_task_wdt_reset();
}
