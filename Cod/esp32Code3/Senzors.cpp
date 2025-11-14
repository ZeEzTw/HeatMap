#include "Senzors.h"

unsigned long totalI2CErrors = 0;
unsigned long lastI2CRecovery = 0;
bool i2cBusLocked = false;
unsigned long influxConsecutiveErrors = 0;
unsigned long sensorConsecutiveFailures = 0;

// Define the I2C instance that was declared as extern in the header
TwoWire I2C = TwoWire(0);

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
  delay(50);
  
  return result;
}



// Enhanced sensor reading with comprehensive error handling
SensorData readSensorWithRecovery(int sda, int scl, int maxTries)
{
  SensorData data = {NAN, NAN};

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
        data.temperature > -80 && data.temperature < 130 &&  // Reasonable temp range
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
      delayMicroseconds(100);
      digitalWrite(scl, LOW);
      delayMicroseconds(100);
    }
  }
  
  // Generate proper I2C STOP condition
  pinMode(sda, OUTPUT);
  digitalWrite(sda, LOW);
  delayMicroseconds(100);
  digitalWrite(scl, HIGH);
  delayMicroseconds(100);
  digitalWrite(sda, HIGH);
  delayMicroseconds(100);
  
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