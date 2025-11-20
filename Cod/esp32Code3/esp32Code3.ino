#include <time.h>
#include "Telegram.h"
#include "InfluxDB.h"
#include "BitBangAHT21ELI.h"
using namespace std;

// Scalable array of bit-banged sensor instances. Size follows NUM_I2C_SENSORS.
BitBangAHT21ELI* bbSensors[NUM_I2C_SENSORS] = { nullptr };


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
  Serial.print("WIFI_SSID: ");
  Serial.println(WIFI_SSID);
  Serial.print("WiFi Password: ");
  Serial.println(WIFI_PASSWORD.length() > 0 ? "[SET]" : "[NOT SET]");
  Serial.print("InfluxDB Host: ");
  Serial.println(INFLUX_HOST);
  Serial.print("Number of I2C Sensors: ");
  Serial.println(NUM_I2C_SENSORS);
  
  // Initialize WiFi with enhanced connection handling
  Serial.println("\n--- WiFi Connection ---");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID.c_str(), WIFI_PASSWORD.c_str());
  
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
  configTime(0, 0, "pool.ntp.INFLUX_ORG", "time.nist.gov");
  
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
  for (int i = 0; i < NUM_I2C_SENSORS; i++) {
    Serial.print("Sensor ");
    Serial.print(i + 1);
    Serial.print(": ID=");
    Serial.print(SENSOR_IDS[i]);
    Serial.print(", SDA=");
    Serial.print(SDA_PINS[i]);
    Serial.print(", SCL=");
    Serial.println(SCL_PINS[i]);
  }

  // Instantiate bit-banged sensor objects for each configured sensor
  for (int i = 0; i < NUM_I2C_SENSORS; i++) {
    Serial.print("Creating BitBangAHT21ELI for SDA=");
    Serial.print(SDA_PINS[i]);
    Serial.print(" SCL=");
    Serial.println(SCL_PINS[i]);
    bbSensors[i] = new BitBangAHT21ELI(SDA_PINS[i], SCL_PINS[i]);
    delay(50);
  }

  // Send startup notification to Telegram
  if (!resetMessageSent && WiFi.status() == WL_CONNECTED) {
    Serial.println("\n--- Startup Notification ---");
    String startupMessage = "🚀 ESP32 Sensor System Started!\n";
    startupMessage += "Reset reason: " + String(esp_reset_reason()) + "\n";
    startupMessage += "Sensors: " + String(NUM_I2C_SENSORS) + "\n";
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
  for (int i = 0; i < NUM_I2C_SENSORS; i++) {
    Serial.print("\n--- Reading sensor ");
    Serial.print(i + 1);
    Serial.print(" (ID: ");
    Serial.print(SENSOR_IDS[i]);
    Serial.println(") ---");
    
    // Reset watchdog before each sensor read
    esp_task_wdt_reset();
    
    unsigned long sensorStartTime = millis();
    
      float tVal = NAN, hVal = NAN;
      bool readOk = false;
      if (i < NUM_I2C_SENSORS && bbSensors[i] != nullptr) {
        readOk = bbSensors[i]->aht_get_data(&tVal, &hVal);
      } else {
        Serial.println("Sensor instance not available");
      }
      unsigned long sensorDuration = millis() - sensorStartTime;

      temp[i] = tVal;
      hum[i] = hVal;

      // Validate sensor data
      if (readOk && !isnan(temp[i]) && !isnan(hum[i]) && 
          temp[i] > -80 && temp[i] < 130 &&
          hum[i] >= 0 && hum[i] <= 100) {

        validSensors++;
        Serial.print("Valid data - Temp: ");
        Serial.print(temp[i], 2);
        Serial.print("\u00b0C, Hum: ");
        Serial.print(hum[i], 2);
        Serial.print("% (");
        Serial.print(sensorDuration);
        Serial.println("ms)");
      
      } else {
        failedSensors++;
        totalI2CErrors++; // count failures
        blinkError(1);
        Serial.print(" Invalid or missing data - Temp: ");
        Serial.print(temp[i]);
        Serial.print(", Hum: ");
        Serial.print(hum[i]);
        Serial.print(" (");
        Serial.print(sensorDuration);
        Serial.println("ms)");
      }
    
    // Delay between sensors (except after last sensor)
    if (i < NUM_I2C_SENSORS - 1) {
      delay(sensorDelayMs);
    }
  }
  
  // Reset watchdog after all sensor readings
  esp_task_wdt_reset();
  
  // Check sensor health
  Serial.print("\nSensor summary: ");
  Serial.print(validSensors);
  Serial.print("/");
  Serial.print(NUM_I2C_SENSORS);
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
  for (int i = 0; i < NUM_I2C_SENSORS; i++) {
    if (!isnan(temp[i]) && !isnan(hum[i]) && 
        temp[i] > -80 && temp[i] < 130 &&
        hum[i] >= 0 && hum[i] <= 100) {
      body += "temperature,device_id=" + SENSOR_IDS[i] + " value=" + String(temp[i], 2) + "\n";
      body += "humidity,device_id=" + SENSOR_IDS[i] + " value=" + String(hum[i], 2) + "\n";
    }
    else
    {
      sendTelegramMessage("Sensor ID " + SENSOR_IDS[i] + " failed to provide valid data check.");
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
                        " Valid_sensors:" + String(validSensors) + "/" + String(NUM_I2C_SENSORS);
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
