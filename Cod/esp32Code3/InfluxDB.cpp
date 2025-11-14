#include "InfluxDB.h"

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
  
  // FIXED: Correct URL parameter names (org= and bucket=, not INFLUX_ORG= and INFLUX_BUCKET=)
  String url = INFLUX_HOST + "/api/v2/write?org=" + INFLUX_ORG + "&bucket=" + INFLUX_BUCKET + "&precision=s";
  
  Serial.println("Sending to InfluxDB:");
  Serial.print("URL: ");
  Serial.println(url);
  Serial.print("Body: ");
  Serial.println(body);
  
  bool http_begin_success = http.begin(url);
  if (!http_begin_success) {
    Serial.println("Failed to begin HTTP connection");
    influxConsecutiveErrors++;
    http.end();
    return;
  }
  
  // FIXED: Correct authorization header format (Token, not INFLUX_TOKEN)
  http.addHeader("Authorization", "Token " + INFLUX_TOKEN);
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
    
    String debugUrl = INFLUX_HOST + "/api/v2/write?org=" + INFLUX_ORG + "&bucket=" + INFLUX_BUCKET + "&precision=s";
    
    if (debugHttp.begin(debugUrl)) {
      debugHttp.addHeader("Authorization", "Token " + INFLUX_TOKEN);
      debugHttp.addHeader("Content-Type", "text/plain");
      debugHttp.addHeader("Connection", "close");
      debugHttp.POST(debugBody);
      debugHttp.end();
    }
  }
}
