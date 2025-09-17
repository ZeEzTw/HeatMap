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

  if (client.connect("api.telegram.INFLUX_ORG", 443)) {
    String encodedMessage = message;
    encodedMessage.replace(" ", "%20");
    encodedMessage.replace("\n", "%0A");
    
    String url = "/bot" + botToken + "/sendMessage?chat_id=" + chatID + "&text=" + encodedMessage;
    
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: api.telegram.INFLUX_ORG\r\n" +
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