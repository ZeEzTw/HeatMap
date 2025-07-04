#include <Wire.h>
#include <SoftwareWire.h>
#include <Adafruit_AHTX0.h>

Adafruit_AHTX0 aht1; // hardware I2C #0
Adafruit_AHTX0 aht2; // hardware I2C #1
Adafruit_AHTX0 aht3; // software I2C
Adafruit_AHTX0 aht4; // software I2C

TwoWire I2C_HW_1 = TwoWire(0);  // pinii 21,22
TwoWire I2C_HW_2 = TwoWire(1);  // pinii 26,27

SoftwareWire myWire1(25, 33);   // SDA, SCL
SoftwareWire myWire2(15, 2);    // SDA, SCL

void setup() {
  Serial.begin(115200);

  // Pornim magistralele
  I2C_HW_1.begin(21, 22);
  I2C_HW_2.begin(26, 27);
  myWire1.begin();
  myWire2.begin();

  // Inițializăm senzorii
  if (!aht1.begin(&I2C_HW_1))
    Serial.println("❌ Eroare senzor 1");
  else
    Serial.println("✅ Senzor 1 ok");

  if (!aht2.begin(&I2C_HW_2))
    Serial.println("❌ Eroare senzor 2");
  else
    Serial.println("✅ Senzor 2 ok");

  if (!aht3.begin(&swWire1))
    Serial.println("❌ Eroare senzor 3");
  else
    Serial.println("✅ Senzor 3 ok");

  if (!aht4.begin(&swWire2))
    Serial.println("❌ Eroare senzor 4");
  else
    Serial.println("✅ Senzor 4 ok");
}

void loop() {
  sensors_event_t humidity, temperature;

  aht1.getEvent(&humidity, &temperature);
  Serial.printf("S1: %.2f°C, %.2f%%\n", temperature.temperature, humidity.relative_humidity);

  delay(500);
  aht2.getEvent(&humidity, &temperature);
  Serial.printf("S2: %.2f°C, %.2f%%\n", temperature.temperature, humidity.relative_humidity);

  delay(500);
  myWire1.getEvent(&humidity, &temperature);
  myWire1.read();
  Serial.printf("S3: %.2f°C, %.2f%%\n", temperature.temperature, humidity.relative_humidity);

  delay(500);
  myWire2.getEvent(&humidity, &temperature);
  myWire2.read();
  Serial.printf("S4: %.2f°C, %.2f%%\n", temperature.temperature, humidity.relative_humidity);

  delay(2000);
}
