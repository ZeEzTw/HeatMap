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

struct SensorData
{
  float temperature;
  float humidity;
};

#define WDT_TIMEOUT 120  // seconds - increased timeout
int sensorDelayMs = 800; // increased for long wires and stability
int cycleDelayMs = 8000; // increased cycle delay to prevent I2C conflicts
#define MAX_I2C_RETRIES 3
#define MAX_SENSOR_FAILURES 5
#define I2C_RECOVERY_DELAY 100
#define SENSOR_INIT_DELAY 50

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

// Global variables for error tracking and recovery
static unsigned long totalI2CErrors = 0;
static unsigned long lastI2CRecovery = 0;
static bool i2cBusLocked = false;
static unsigned long influxConsecutiveErrors = 0;
static unsigned long sensorConsecutiveFailures = 0;

// Global variables for tracking
int once = 0;
bool resetMessageSent = false;

// Enhanced I2C bus recovery with better error handling
bool i2cBusRecovery(int scl, int sda)
{
  Serial.println("Starting I2C bus recovery...");
  
  // Prevent multiple recovery attempts too close together
  if (millis() - lastI2CRecovery < 1000) {
    Serial.println("Recovery attempt too soon, skipping...");
    return false;
  }
  lastI2CRecovery = millis();
  
  // End current I2C session
  I2C.end();
  delay(50);
  
  // Set pins as GPIO
  pinMode(scl, OUTPUT);
  pinMode(sda, INPUT_PULLUP);
  
  // Check if SDA is stuck low
  if (digitalRead(sda) == LOW) {
    Serial.println("SDA stuck low, attempting clock pulses...");
    // Generate clock pulses to release stuck device
    for (int i = 0; i < 20 && digitalRead(sda) == LOW; i++) {
      digitalWrite(scl, HIGH);
      delayMicroseconds(10);
      digitalWrite(scl, LOW);
      delayMicroseconds(10);
    }
  }
  
  // Generate proper I2C STOP condition
  pinMode(sda, OUTPUT);
  digitalWrite(sda, LOW);
  delayMicroseconds(10);
  digitalWrite(scl, HIGH);
  delayMicroseconds(10);
  digitalWrite(sda, HIGH);
  delayMicroseconds(10);
  
  // Reset pins to input with pullup
  pinMode(sda, INPUT_PULLUP);
  pinMode(scl, INPUT_PULLUP);
  delay(I2C_RECOVERY_DELAY);
  
  // Reinitialize I2C with longer timeout
  I2C.begin(sda, scl);
  I2C.setClock(10000); // Very low speed for reliability
  I2C.setTimeout(1000); // 1 second timeout
  delay(SENSOR_INIT_DELAY);
  
  bool recovery_success = (digitalRead(sda) == HIGH && digitalRead(scl) == HIGH);
  Serial.print("I2C recovery ");
  Serial.println(recovery_success ? "successful" : "failed");
  
  if (!recovery_success) {
    i2cBusLocked = true;
    totalI2CErrors++;
  } else {
    i2cBusLocked = false;
  }
  
  return recovery_success;
}

// Enhanced restart function with error reporting
void restartESP()
{
  Serial.println("=== ESP RESTART INITIATED ===");
  Serial.print("Total I2C errors: ");
  Serial.println(totalI2CErrors);
  Serial.print("Influx consecutive errors: ");
  Serial.println(influxConsecutiveErrors);
  Serial.print("Sensor consecutive failures: ");
  Serial.println(sensorConsecutiveFailures);
  
  // Send restart notification
  String restartMsg = "ESP32 RESTART - I2C_errors:" + String(totalI2CErrors) + 
                     " Influx_errors:" + String(influxConsecutiveErrors) + 
                     " Sensor_failures:" + String(sensorConsecutiveFailures);
  sendTelegramMessage(restartMsg);
  
  // Cleanup before restart
  I2C.end();
  WiFi.disconnect();
  esp_task_wdt_delete(NULL);
  
  delay(2000);
  ESP.restart();
}

// Enhanced sensor reading with comprehensive error handling
SensorData readSensorWithRecovery(int sda, int scl, int maxTries = MAX_I2C_RETRIES)
{
  SensorData data = {NAN, NAN};
  
  // Skip if I2C bus is locked from previous failures
  if (i2cBusLocked) {
    Serial.println("I2C bus locked, skipping sensor read");
    return data;
  }
  
  // Feed watchdog before starting sensor read
  esp_task_wdt_reset();
  
  for (int attempt = 0; attempt < maxTries; attempt++) {
    Serial.print("Sensor read attempt ");
    Serial.print(attempt + 1);
    Serial.print("/");
    Serial.println(maxTries);
    
    data = readSensor(sda, scl);
    
    // Check if reading is valid
    if (!isnan(data.temperature) && !isnan(data.humidity) && 
        data.temperature > -40 && data.temperature < 85 &&  // Reasonable temp range
        data.humidity >= 0 && data.humidity <= 100) {       // Valid humidity range
      Serial.println("Valid sensor reading obtained");
      sensorConsecutiveFailures = 0; // Reset failure counter on success
      return data;
    }
    
    // Reading failed, increment error counter
    totalI2CErrors++;
    Serial.print("Invalid reading - Temp: ");
    Serial.print(data.temperature);
    Serial.print(", Hum: ");
    Serial.println(data.humidity);
    
    // Don't attempt recovery on last try
    if (attempt < maxTries - 1) {
      Serial.println("Attempting I2C recovery...");
      
      // Try I2C recovery
      if (!i2cBusRecovery(scl, sda)) {
        Serial.println("I2C recovery failed, trying different approach...");
        
        // Alternative recovery: complete pin reset
        delay(100);
        I2C.end();
        delay(200);
        I2C.begin(sda, scl);
        I2C.setClock(10000);
        I2C.setTimeout(2000);
        delay(100);
      }
      
      // Delay between attempts
      delay(sensorDelayMs);
      esp_task_wdt_reset(); // Feed watchdog between attempts
    }
  }
  
  // All attempts failed
  sensorConsecutiveFailures++;
  Serial.print("All sensor read attempts failed. Consecutive failures: ");
  Serial.println(sensorConsecutiveFailures);
  
  return data; // Return NAN values
}
// Enhanced monitoring and logging function
void logSystemStatus(unsigned long i2cErrorCount)
{
  Serial.println("\n=== SYSTEM STATUS REPORT ===");
  Serial.print("Uptime: ");
  Serial.print(millis() / 1000);
  Serial.println(" seconds");
  Serial.print("Free Heap: ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");
  Serial.print("WiFi RSSI: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
  Serial.print("WiFi Status: ");
  Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
  Serial.print("Reset Reason: ");
  Serial.println(esp_reset_reason());
  Serial.print("Total I2C Errors: ");
  Serial.println(i2cErrorCount);
  Serial.print("InfluxDB Consecutive Errors: ");
  Serial.println(influxConsecutiveErrors);
  Serial.print("Sensor Consecutive Failures: ");
  Serial.println(sensorConsecutiveFailures);
  Serial.print("I2C Bus Status: ");
  Serial.println(i2cBusLocked ? "LOCKED" : "OK");
  Serial.println("============================\n");
  
  // Send startup debug info only once
  String debugBody;
  if (once == 0) {
    debugBody = "debug,device_id=ESP32 reset_reason=" + String(esp_reset_reason()) + 
               ",startup=1,free_heap=" + String(ESP.getFreeHeap());
    sendTelegramMessage("ESP32 Started - Reset reason: " + String(esp_reset_reason()));
    once++;
  } else {
    debugBody = "debug,device_id=ESP32";
  }
  
  // Add comprehensive system metrics
  debugBody += ",uptime=" + String(millis() / 1000) +
              ",wifi_rssi=" + String(WiFi.RSSI()) + 
              ",i2c_error_count=" + String(i2cErrorCount) +
              ",influx_consecutive_errors=" + String(influxConsecutiveErrors) +
              ",sensor_consecutive_failures=" + String(sensorConsecutiveFailures) +
              ",free_heap=" + String(ESP.getFreeHeap()) +
              ",wifi_connected=" + String(WiFi.status() == WL_CONNECTED ? 1 : 0) +
              ",i2c_bus_locked=" + String(i2cBusLocked ? 1 : 0);
  
  // Send debug data to InfluxDB only if WiFi is connected
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient debugHttp;
    debugHttp.setTimeout(5000);
    if (debugHttp.begin(influx_host + "/api/v2/write?org=" + org + "&bucket=" + bucket + "&precision=s")) {
      debugHttp.addHeader("Authorization", "Token " + token);
      debugHttp.addHeader("Content-Type", "text/plain");
      debugHttp.addHeader("Connection", "close");
      int debugStatus = debugHttp.POST(debugBody);
      debugHttp.end();
      
      Serial.print("Debug data sent to InfluxDB, status: ");
      Serial.println(debugStatus);
    }
  }
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n=== ESP32 SENSOR MONITORING SYSTEM ===");
  Serial.println("Initializing system...");
  
  // Configure enhanced watchdog timer
  Serial.println("Configuring Watchdog Timer...");
  Serial.print("Watchdog Timeout: ");
  Serial.print(WDT_TIMEOUT);
  Serial.println(" seconds");
  
  // Disable any existing WDT to prevent conflicts
  esp_task_wdt_deinit();
  delay(100);

  // Configure WDT with enhanced settings
  esp_task_wdt_config_t wdt_config = {
      .timeout_ms = WDT_TIMEOUT * 1000,                // timeout in ms
      .idle_core_mask = (1 << portNUM_PROCESSORS) - 1, // all CPU cores
      .trigger_panic = true                            // panic -> restart
  };

  // Initialize WDT with error handling
  esp_err_t wdt_init_result = esp_task_wdt_init(&wdt_config);
  if (wdt_init_result != ESP_OK) {
    Serial.print("ERROR: Failed to initialize watchdog timer, code: ");
    Serial.println(wdt_init_result);
    delay(1000);
    ESP.restart();
  }

  // Add current task to watchdog with error handling
  esp_err_t wdt_add_result = esp_task_wdt_add(NULL);
  if (wdt_add_result != ESP_OK) {
    Serial.print("ERROR: Failed to add task to watchdog timer, code: ");
    Serial.println(wdt_add_result);
    delay(1000);
    ESP.restart();
  }
  
  Serial.println("Watchdog configured successfully");

  // Display network configuration
  Serial.println("\n--- Network Configuration ---");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("WiFi Password: ");
  Serial.println(password.length() > 0 ? "[SET]" : "[NOT SET]");
  Serial.print("InfluxDB Host: ");
  Serial.println(influx_host);
  Serial.print("Number of I2C Sensors: ");
  Serial.println(numI2CSensors);
  
  // Initialize WiFi with enhanced connection handling
  Serial.println("\n--- WiFi Connection ---");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  Serial.print("Connecting to WiFi");
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 30) {
    blinkError(1);
    delay(1000);
    Serial.print(".");
    wifiAttempts++;
    esp_task_wdt_reset(); // Feed watchdog during WiFi connection
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected successfully!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("\nERROR: Failed to connect to WiFi!");
    Serial.println("Restarting in 5 seconds...");
    delay(5000);
    ESP.restart();
  }

  // SNTP time synchronization with enhanced error handling
  Serial.println("\n--- Time Synchronization ---");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  
  Serial.print("Waiting for SNTP synchronization");
  time_t now = time(nullptr);
  int timeAttempts = 0;
  while (now < 1000000 && timeAttempts < 30) { // Wait for valid timestamp
    delay(1000);
    now = time(nullptr);
    Serial.print(".");
    timeAttempts++;
    esp_task_wdt_reset();
  }
  
  if (now >= 1000000) {
    Serial.println("\nTime synchronized successfully!");
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    Serial.print("Current time: ");
    Serial.println(asctime(&timeinfo));
  } else {
    Serial.println("\nWARNING: Time synchronization failed, continuing anyway...");
  }

  // Initialize sensor configuration
  Serial.println("\n--- Sensor Configuration ---");
  for (int i = 0; i < numI2CSensors; i++) {
    Serial.print("Sensor ");
    Serial.print(i + 1);
    Serial.print(": ID=");
    Serial.print(sensorIDs[i]);
    Serial.print(", SDA=");
    Serial.print(sdaPin[i]);
    Serial.print(", SCL=");
    Serial.println(sclPin[i]);
  }

  // Send startup notification to Telegram
  if (!resetMessageSent && WiFi.status() == WL_CONNECTED) {
    Serial.println("\n--- Startup Notification ---");
    String startupMessage = "🚀 ESP32 Sensor System Started!\n";
    startupMessage += "Reset reason: " + String(esp_reset_reason()) + "\n";
    startupMessage += "Sensors: " + String(numI2CSensors) + "\n";
    startupMessage += "Free memory: " + String(ESP.getFreeHeap()) + " bytes\n";
    startupMessage += "WiFi RSSI: " + String(WiFi.RSSI()) + " dBm";
    
    sendTelegramMessage(startupMessage);
    resetMessageSent = true;
    Serial.println("Startup notification sent");
  }

  Serial.println("\n=== SYSTEM INITIALIZATION COMPLETE ===");
  Serial.println("Starting main sensor monitoring loop...\n");
  delay(1000);
}

// Improved sensor reading with better error handling and timeouts
SensorData readSensor(int sda, int scl)
{
  SensorData result = {NAN, NAN};
  
  // End any previous I2C session
  I2C.end();
  delay(20);
  
  // Initialize I2C with conservative settings
  I2C.begin(sda, scl);
  I2C.setClock(10000); // 10kHz for maximum reliability with long wires
  I2C.setTimeout(2000); // 2 second timeout
  delay(SENSOR_INIT_DELAY);
  
  // Check I2C bus health before proceeding
  I2C.beginTransmission(0x38); // AHT20 default address
  uint8_t i2c_error = I2C.endTransmission();
  
  if (i2c_error != 0) {
    Serial.print("I2C bus check failed with error: ");
    Serial.println(i2c_error);
    I2C.end();
    return result;
  }
  
  Adafruit_AHTX0 aht;
  sensors_event_t humidity, temperature;

  // Attempt sensor initialization with timeout
  bool sensor_init_success = false;
  unsigned long init_start = millis();
  
  while (!sensor_init_success && (millis() - init_start) < 3000) {
    sensor_init_success = aht.begin(&I2C);
    if (!sensor_init_success) {
      delay(100);
      esp_task_wdt_reset(); // Feed watchdog during init attempts
    }
  }

  if (sensor_init_success) {
    Serial.println("Sensor initialized successfully");
    delay(50); // Allow sensor to stabilize
    
    // Attempt to read data with timeout protection
    unsigned long read_start = millis();
    bool read_success = false;
    
    while (!read_success && (millis() - read_start) < 2000) {
      aht.getEvent(&humidity, &temperature);
      
      // Validate readings
      if (!isnan(temperature.temperature) && !isnan(humidity.relative_humidity)) {
        result.temperature = temperature.temperature;
        result.humidity = humidity.relative_humidity;
        read_success = true;
      } else {
        delay(100);
        esp_task_wdt_reset();
      }
    }
    
    if (!read_success) {
      Serial.println("Sensor read timeout");
    }
  } else {
    Serial.println("Sensor initialization failed after timeout");
  }

  // Always end I2C session
  I2C.end();
  delay(10);
  
  return result;
}

// Enhanced WiFi reconnection with better error handling
void reconnectWiFi()
{
  static unsigned long lastReconnectAttempt = 0;
  static int reconnectAttempts = 0;
  
  // Prevent too frequent reconnection attempts
  if (millis() - lastReconnectAttempt < 10000) {
    return;
  }
  lastReconnectAttempt = millis();
  reconnectAttempts++;
  
  blinkError(3);
  Serial.print("WiFi disconnected! Reconnection attempt #");
  Serial.println(reconnectAttempts);
  
  // Complete WiFi reset
  WiFi.mode(WIFI_OFF);
  delay(1000);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(500);
  
  WiFi.begin(ssid.c_str(), password.c_str());
  
  // Wait for connection with timeout
  unsigned long connect_start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - connect_start) < 15000) {
    delay(500);
    Serial.print(".");
    esp_task_wdt_reset();
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi reconnected successfully!");
    reconnectAttempts = 0;
  } else {
    Serial.println("\nFailed to reconnect WiFi!");
    
    // If multiple reconnection attempts fail, restart ESP
    if (reconnectAttempts >= 3) {
      Serial.println("Multiple WiFi reconnection failures, restarting ESP...");
      restartESP();
    }
  }
}

// Enhanced InfluxDB writing with comprehensive error handling
void writeToInfluxDB(const String &body)
{
  // Skip if no data to send
  if (body.length() == 0) {
    Serial.println("No data to send to InfluxDB");
    return;
  }
  
  // Check WiFi connection first
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, attempting reconnection...");
    influxConsecutiveErrors++;
    reconnectWiFi();
    
    // Don't attempt InfluxDB write if WiFi still not connected
    if (WiFi.status() != WL_CONNECTED) {
      return;
    }
  }
  
  HTTPClient http;
  http.setTimeout(10000); // 10 second timeout
  http.setConnectTimeout(5000); // 5 second connection timeout
  
  String url = influx_host + "/api/v2/write?org=" + org + "&bucket=" + bucket + "&precision=s";
  
  Serial.println("Sending to InfluxDB:");
  Serial.println(body);
  
  bool http_begin_success = http.begin(url);
  if (!http_begin_success) {
    Serial.println("Failed to begin HTTP connection");
    influxConsecutiveErrors++;
    http.end();
    return;
  }
  
  http.addHeader("Authorization", "Token " + token);
  http.addHeader("Content-Type", "text/plain");
  http.addHeader("Connection", "close");
  
  // Feed watchdog before HTTP request
  esp_task_wdt_reset();
  
  int httpResponseCode = http.POST(body);
  Serial.print("HTTP Response Code: ");
  Serial.println(httpResponseCode);
  
  // Check response
  if (httpResponseCode == 204) {
    // Success
    influxConsecutiveErrors = 0;
    Serial.println("Data sent successfully to InfluxDB");
  } else if (httpResponseCode == -1) {
    // Connection timeout or failure
    influxConsecutiveErrors++;
    Serial.println("HTTP connection failed (error -1)");
    
    // Try to get more detailed error info
    String error = http.errorToString(httpResponseCode);
    Serial.print("Error details: ");
    Serial.println(error);
    
  } else if (httpResponseCode == -3) {
    // Connection lost
    influxConsecutiveErrors++;
    Serial.println("HTTP connection lost (error -3)");
    
  } else {
    // Other HTTP errors
    influxConsecutiveErrors++;
    Serial.print("HTTP error: ");
    Serial.println(httpResponseCode);
    
    // Try to get response body for debugging
    if (http.getSize() > 0) {
      String response = http.getString();
      Serial.print("Response: ");
      Serial.println(response);
    }
  }
  
  http.end();
  
  // Handle consecutive errors
  Serial.print("InfluxDB consecutive errors: ");
  Serial.println(influxConsecutiveErrors);
  
  if (influxConsecutiveErrors >= 5) {
    Serial.println("Too many InfluxDB errors, attempting WiFi reconnection...");
    reconnectWiFi();
    
    if (influxConsecutiveErrors >= 8) {
      Serial.println("Critical InfluxDB failure, restarting ESP...");
      restartESP();
    }
  }
  
  // Send debug information (only if main request didn't completely fail)
  if (httpResponseCode != -1 && httpResponseCode != -3) {
    String debugBody = "debug,device_id=ESP32 influx_error_count=" + String(influxConsecutiveErrors) + 
                      ",wifi_rssi=" + String(WiFi.RSSI()) + ",http_response=" + String(httpResponseCode);
    
    HTTPClient debugHttp;
    debugHttp.setTimeout(5000);
    if (debugHttp.begin(influx_host + "/api/v2/write?org=" + org + "&bucket=" + bucket + "&precision=s")) {
      debugHttp.addHeader("Authorization", "Token " + token);
      debugHttp.addHeader("Content-Type", "text/plain");
      debugHttp.addHeader("Connection", "close");
      debugHttp.POST(debugBody);
      debugHttp.end();
    }
  }
}

// Enhanced Telegram messaging with error handling
void sendTelegramMessage(const String &message)
{
  const String botToken = "8433695203:AAGs6I-c5zyHJFurlDWAW6m8qniScs7Uq-g";
  const String chatID = "5291138966";

  // Check WiFi first
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, cannot send Telegram message");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure(); // Disable SSL certificate validation
  client.setTimeout(10000); // 10 second timeout

  Serial.print("Sending Telegram message: ");
  Serial.println(message);

  if (client.connect("api.telegram.org", 443)) {
    String encodedMessage = message;
    encodedMessage.replace(" ", "%20");
    encodedMessage.replace("\n", "%0A");
    
    String url = "/bot" + botToken + "/sendMessage?chat_id=" + chatID + "&text=" + encodedMessage;
    
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: api.telegram.org\r\n" +
                 "User-Agent: ESP32\r\n" +
                 "Connection: close\r\n\r\n");
    
    // Wait for response with timeout
    unsigned long timeout = millis();
    while (client.connected() && millis() - timeout < 5000) {
      String line = client.readStringUntil('\n');
      if (line == "\r") {
        break;
      }
    }
    
    client.stop();
    Serial.println("Telegram message sent successfully");
  } else {
    Serial.println("Failed to connect to Telegram API");
  }
}

void loop()
{
  static unsigned long lastLog = 0;
  static unsigned long loopCounter = 0;
  static unsigned long lastWiFiCheck = 0;
  
  loopCounter++;
  
  // Reset watchdog at start of loop
  esp_task_wdt_reset();
  
  // Periodic WiFi health check
  if (millis() - lastWiFiCheck > 30000) { // Check every 30 seconds
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi connection lost, attempting reconnection...");
      reconnectWiFi();
    }
    lastWiFiCheck = millis();
  }
  
  Serial.print("\n=== LOOP ");
  Serial.print(loopCounter);
  Serial.println(" START ===");
  
  float temp[5] = {NAN}, hum[5] = {NAN};
  int validSensors = 0;
  int failedSensors = 0;
  
  // Read all configured sensors
  for (int i = 0; i < numI2CSensors; i++) {
    Serial.print("\n--- Reading sensor ");
    Serial.print(i + 1);
    Serial.print(" (ID: ");
    Serial.print(sensorIDs[i]);
    Serial.println(") ---");
    
    // Reset watchdog before each sensor read
    esp_task_wdt_reset();
    
    unsigned long sensorStartTime = millis();
    SensorData data = readSensorWithRecovery(sdaPin[i], sclPin[i]);
    unsigned long sensorDuration = millis() - sensorStartTime;
    
    temp[i] = data.temperature;
    hum[i] = data.humidity;
    
    // Validate sensor data
    if (!isnan(temp[i]) && !isnan(hum[i]) && 
        temp[i] > -40 && temp[i] < 85 &&
        hum[i] >= 0 && hum[i] <= 100) {
      
      validSensors++;
      Serial.print("✓ Valid data - Temp: ");
      Serial.print(temp[i], 2);
      Serial.print("°C, Hum: ");
      Serial.print(hum[i], 2);
      Serial.print("% (");
      Serial.print(sensorDuration);
      Serial.println("ms)");
      
    } else {
      failedSensors++;
      blinkError(1);
      Serial.print("✗ Invalid data - Temp: ");
      Serial.print(temp[i]);
      Serial.print(", Hum: ");
      Serial.print(hum[i]);
      Serial.print(" (");
      Serial.print(sensorDuration);
      Serial.println("ms)");
    }
    
    // Delay between sensors (except after last sensor)
    if (i < numI2CSensors - 1) {
      delay(sensorDelayMs);
    }
  }
  
  // Reset watchdog after all sensor readings
  esp_task_wdt_reset();
  
  // Check sensor health
  Serial.print("\nSensor summary: ");
  Serial.print(validSensors);
  Serial.print("/");
  Serial.print(numI2CSensors);
  Serial.println(" sensors OK");
  
  // Handle complete sensor failure
  if (validSensors == 0) {
    sensorConsecutiveFailures++;
    Serial.print("All sensors failed! Consecutive failures: ");
    Serial.println(sensorConsecutiveFailures);
    
    if (sensorConsecutiveFailures >= MAX_SENSOR_FAILURES) {
      Serial.println("Too many consecutive sensor failures, restarting ESP...");
      String failureMsg = "ESP32 - ALL SENSORS FAILED " + String(MAX_SENSOR_FAILURES) + " times consecutively!";
      sendTelegramMessage(failureMsg);
      restartESP();
    }
  } else {
    // Reset failure counter if at least one sensor works
    if (sensorConsecutiveFailures > 0) {
      Serial.print("Sensor recovery detected, resetting failure counter from ");
      Serial.println(sensorConsecutiveFailures);
      sensorConsecutiveFailures = 0;
    }
  }
  
  // Build InfluxDB payload only for valid readings
  String body = "";
  for (int i = 0; i < numI2CSensors; i++) {
    if (!isnan(temp[i]) && !isnan(hum[i]) && 
        temp[i] > -40 && temp[i] < 85 &&
        hum[i] >= 0 && hum[i] <= 100) {
      body += "temperature,device_id=" + sensorIDs[i] + " value=" + String(temp[i], 2) + "\n";
      body += "humidity,device_id=" + sensorIDs[i] + " value=" + String(hum[i], 2) + "\n";
    }
  }
  
  // Remove trailing newline
  if (body.length() > 0 && body.endsWith("\n")) {
    body.remove(body.length() - 1);
  }
  
  // Send data to InfluxDB
  if (body.length() > 0) {
    Serial.println("\nSending data to InfluxDB...");
    writeToInfluxDB(body);
  } else {
    Serial.println("\nNo valid data to send to InfluxDB");
  }
  
  // Reset watchdog after InfluxDB operation
  esp_task_wdt_reset();
  
  // Periodic system monitoring and logging
  if (millis() - lastLog > 60000) { // Every 60 seconds
    logSystemStatus(totalI2CErrors);
    lastLog = millis();
    
    // Send periodic health report
    if (loopCounter % 50 == 0) { // Every ~50 loops (approximately every 7-8 minutes)
      String healthMsg = "ESP32 Health - Loop:" + String(loopCounter) + 
                        " I2C_errors:" + String(totalI2CErrors) + 
                        " Valid_sensors:" + String(validSensors) + "/" + String(numI2CSensors);
      sendTelegramMessage(healthMsg);
    }
  }
  
  Serial.print("=== LOOP ");
  Serial.print(loopCounter);
  Serial.print(" END (duration: ");
  Serial.print(millis() % 100000); // Show relative time
  Serial.println("ms) ===\n");
  
  // Final delay before next cycle
  delay(cycleDelayMs);
  esp_task_wdt_reset();
}