#include "Telegram.h"

// Enhanced Telegram messaging with error handling
void sendTelegramMessage(const String &message)
{
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
    // URL encode the message
    String encodedMessage = message;
    encodedMessage.replace(" ", "%20");
    encodedMessage.replace("\n", "%0A");
    encodedMessage.replace("&", "%26");
    encodedMessage.replace("#", "%23");
    encodedMessage.replace("+", "%2B");
    
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
    
    // Optional: Read and print response for debugging
    String response = "";
    timeout = millis();
    while (client.available() && millis() - timeout < 2000) {
      response += client.readString();
    }
    
    client.stop();
    
    if (response.indexOf("\"ok\":true") > 0) {
      Serial.println("Telegram message sent successfully");
    } else {
      Serial.println("Telegram API returned error");
      Serial.println(response);
    }
  } else {
    Serial.println("Failed to connect to Telegram API");
  }
}