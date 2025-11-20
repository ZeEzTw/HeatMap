#include "Error.h"
#include <WiFi.h>

bool resetMessageSent = false;
int once = 0;

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
  
  // Send restart notification
  String restartMsg = "ESP32 RESTART - I2C_errors:" + String(totalI2CErrors) + 
                     " Influx_errors:" + String(influxConsecutiveErrors) + 
                     " Sensor_failures:" + String(sensorConsecutiveFailures);
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
  Serial.println(esp_reset_reason());
  Serial.print("Total I2C Errors: ");
  Serial.println(i2cErrorCount);
  Serial.print("InfluxDB Consecutive Errors: ");
  Serial.println(influxConsecutiveErrors);
  Serial.print("Sensor Consecutive Failures: ");
  Serial.println(sensorConsecutiveFailures);
  Serial.println("============================\n");
  
  // Send startup debug info only once
  if (once == 0) {
    String startupMsg = "ESP32 Started - Reset reason: " + String(esp_reset_reason()) +
                       ", Free heap: " + String(ESP.getFreeHeap()) + " bytes";
    sendTelegramMessage(startupMsg);
    once++;
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