#include "MQTT.h"

// NTP config values
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

// Blink control vars
bool blinking = false;
unsigned long blinkStartTime = 0;

void blinkLED() {
  static unsigned long lastToggle = 0;
  unsigned long currentMillis = millis();

  if (currentMillis - lastToggle >= 2000) { // toggle LED la 2 secunde
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    lastToggle = currentMillis;
  }
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("🔁 Conectare la MQTT...");
    if (mqttClient.connect(deviceID.c_str())) {
      Serial.println("✅ Conectat la MQTT");
      mqttClient.subscribe(("commands/" + deviceID).c_str());
    } else {
      Serial.print("❌ Eroare MQTT, cod: ");
      Serial.println(mqttClient.state());
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("📥 Comandă primită pe topicul ");
  Serial.println(topic);

  String command = "";
  for (unsigned int i = 0; i < length; i++) {
    command += (char)payload[i];
  }
  Serial.println("🔧 Comandă: " + command);

  if (command == "blink") {
    blinking = true;
    blinkStartTime = millis();
    Serial.println("🔆 Pornit blink LED");
  }
}
