#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>

const char *ssid = "hotspot";
const char *password = "123456789";

// I2C pins for different buses
const int SDA1 = 21;
const int SCL1 = 22;
const int SDA2 = 26;
const int SCL2 = 27;
const int SDA3 = 33;
const int SCL3 = 25;
const int SDA4 = 19;
const int SCL4 = 18;

TwoWire I2C = TwoWire(0); // We'll reuse this I2C instance for all sensors

// Structure to hold temperature and humidity readings
struct SensorData {
  float temperature;
  float humidity;
  bool success;
};

void setup()
{
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.print("Conectare la WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectat!");
}

// Function to read sensor, taking the power pin and I2C pins as arguments
SensorData readSensor(int sdaPin, int sclPin) {
  SensorData result = {0, 0, false};
  Adafruit_AHTX0 aht;
  sensors_event_t humidity, temperature;

  // Configure I2C for this sensor
  I2C.begin(sdaPin, sclPin);
  delay(50);
  
  // Initialize sensor and read data
  if (aht.begin(&I2C)) {
    aht.getEvent(&humidity, &temperature);
    result.temperature = temperature.temperature;
    result.humidity = humidity.relative_humidity;
    result.success = true;
  }
  
  // End I2C connection
  I2C.end();
  
  return result;
}

void loop()
{
  // Read Sensor 1
  SensorData sensor1 = readSensor(SDA1, SCL1);
  if (sensor1.success) {
    Serial.print("Senzor 1 -> Temp: ");
    Serial.print(sensor1.temperature);
    Serial.print(" °C, ");
    Serial.print("Umiditate: ");
    Serial.print(sensor1.humidity);
    Serial.println(" %");
  } else {
    Serial.println("Eroare: AHT21 senzor 1 nu detectat!");
  }
  delay(2000);
  
  // Read Sensor 2
  SensorData sensor2 = readSensor(SDA2, SCL2);
  if (sensor2.success) {
    Serial.print("Senzor 2 -> Temp: ");
    Serial.print(sensor2.temperature);
    Serial.print(" °C, ");
    Serial.print("Umiditate: ");
    Serial.print(sensor2.humidity);
    Serial.println(" %");
  } else {
    Serial.println("Eroare: AHT21 senzor 2 nu detectat!");
  }
  delay(2000);
  
  // Read Sensor 3
  SensorData sensor3 = readSensor(SDA3, SCL3);
  if (sensor3.success) {
    Serial.print("Senzor 3 -> Temp: ");
    Serial.print(sensor3.temperature);
    Serial.print(" °C, ");
    Serial.print("Umiditate: ");
    Serial.print(sensor3.humidity);
    Serial.println(" %");
  } else {
    Serial.println("Eroare: AHT21 senzor 3 nu detectat!");
  }
  delay(2000);
  
  // Read Sensor 4
  SensorData sensor4 = readSensor(SDA4, SCL4);
  if (sensor4.success) {
    Serial.print("Senzor 4 -> Temp: ");
    Serial.print(sensor4.temperature);
    Serial.print(" °C, ");
    Serial.print("Umiditate: ");
    Serial.print(sensor4.humidity);
    Serial.println(" %");
  } else {
    Serial.println("Eroare: AHT21 senzor 4 nu detectat!");
  }
  
  delay(5000); // Main loop delay
}
