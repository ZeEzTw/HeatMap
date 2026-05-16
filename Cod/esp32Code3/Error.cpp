#include "Error.h"
#include <WiFi.h>

bool resetMessageSent = false;
int once = 0;
// Global error tracking variables
unsigned long totalI2CErrors = 0;
unsigned long influxConsecutiveErrors = 0;
unsigned long sensorConsecutiveFailures = 0;


void blinkError(int code, int repeat) {
  pinMode(LED_PIN, OUTPUT);
  for (int r = 0; r < repeat; r++) {
    for (int i = 0; i < code; i++) {
      digitalWrite(LED_PIN, HIGH);
      delay(300);
      digitalWrite(LED_PIN, LOW);
      delay(300);
    }
    delay(1000); // Pauză între cicluri
  }
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
  
  // Log reset reason description
  esp_reset_reason_t rr = esp_reset_reason();
  Serial.print("Reset reason (code): ");
  Serial.print((int)rr);
  Serial.print(" - ");
  Serial.println(resetReasonToString(rr));
  
  // Send restart notification
  String restartMsg = "ESP32 RESTART - I2C_errors:" + String(totalI2CErrors) + 
                     " Influx_errors:" + String(influxConsecutiveErrors) + 
                     " Sensor_failures:" + String(sensorConsecutiveFailures) +
                     " Reset_code:" + String((int)rr) + "(" + resetReasonToString(rr) + ")";
  sendTelegramMessage(restartMsg);
  
  // Cleanup before restart
  WiFi.disconnect();
  
  delay(2000);
  ESP.restart();
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
  esp_reset_reason_t rr = esp_reset_reason();
  Serial.print((int)rr);
  Serial.print(" - ");
  Serial.println(resetReasonToString(rr));
  Serial.print("Total I2C Errors: ");
  Serial.println(i2cErrorCount);
  Serial.print("InfluxDB Consecutive Errors: ");
  Serial.println(influxConsecutiveErrors);
  Serial.print("Sensor Consecutive Failures: ");
  Serial.println(sensorConsecutiveFailures);
  Serial.println("============================\n");
  
  // Send startup debug info only once
  if (once == 0) {
    esp_reset_reason_t rr = esp_reset_reason();
    String startupMsg = "ESP32 Started - Reset_code:" + String((int)rr) + "(" + resetReasonToString(rr) + ")" +
                       ", Free heap: " + String(ESP.getFreeHeap()) + " bytes";
    sendTelegramMessage(startupMsg);
    once++;
  }
}


// Convert esp_reset_reason value to a human-readable description
String resetReasonToString(esp_reset_reason_t reason)
{
  int r = (int)reason;
  switch (r) {
    case 0: return "UNKNOWN - Reset reason could not be determined";              // ESP_RST_UNKNOWN
    case 1: return "POWERON_RESET - Device powered on or power reapplied";        // ESP_RST_POWERON
    case 2: return "EXTERNAL_RESET - External reset (reset pin pressed or external circuit reset; can also happen with power cycling/unplugging)";       // ESP_RST_EXT
    case 3: return "SOFTWARE_RESET - Software-called reset (esp_restart) or software-triggered reboot";       // ESP_RST_SW
    case 4: return "PANIC/Abort - CPU panic or abort, typically an unhandled exception or abort()";          // ESP_RST_PANIC
    case 5: return "INT WDT Reset - Internal watchdog reset (main CPU watchdog timeout)";        // ESP_RST_INT_WDT
    case 6: return "Task WDT Reset - Task watchdog triggered because a task blocked for too long";       // ESP_RST_TASK_WDT
    case 7: return "Other WDT Reset - Other watchdog (RTC or system) triggered a reset";      // ESP_RST_WDT
    case 8: return "DEEPSLEEP_RESET - Woke from deep sleep (normal deep-sleep wakeup)";     // ESP_RST_DEEPSLEEP
    case 9: return "BROWNOUT_RESET - Brownout detector triggered (supply voltage dipped below threshold)";       // ESP_RST_BROWNOUT
    case 10: return "SDIO_RESET - Reset caused by SDIO (host) interface or related event";          // ESP_RST_SDIO
    default: {
      String s = "Unknown(" + String(r) + ")";
      return s;
    }
  }
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
  
  WiFi.begin(WIFI_SSID.c_str(), WIFI_PASSWORD.c_str());
  
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