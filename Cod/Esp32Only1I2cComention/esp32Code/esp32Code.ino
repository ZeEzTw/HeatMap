#include <Wire.h>
#include <Adafruit_AHTX0.h>

// Sensor object
Adafruit_AHTX0 aht;

// Timing
int delaySeconds = 2;

void setup() {
  Serial.begin(115200);
  Serial.println("🚀 Starting ESP32 Sensor Reader...");
  
  // Initialize I2C with pins 21 (SDA) and 22 (SCL)
  Wire.begin(21, 22);
  
  // Initialize AHT21 sensor
  if (!aht.begin()) {
    Serial.println("❌ Error: AHT21 sensor not detected!");
    while (true) {
      delay(1000);
      Serial.println("❌ Sensor check failed, retrying...");
    }
  }
  Serial.println("✅ AHT21 sensor OK");
  Serial.println("� Starting data readings...");
}

void loop() {
  // Read sensor data
  Serial.print("Start");
  sensors_event_t humidity, temperature;
  aht.getEvent(&humidity, &temperature);

  float temp = temperature.temperature;
  float hum = humidity.relative_humidity;

  // Check if readings are valid
  if (isnan(temp) || isnan(hum)) {
    Serial.println("⚠️ Invalid sensor data, retrying...");
    delay(1000);
    return;
  }

  // Print sensor data to serial
  Serial.printf("📊 Temperature: %.2f °C | Humidity: %.2f %%\n", temp, hum);

  // Wait before next reading
  delay(delaySeconds * 1000);
}


