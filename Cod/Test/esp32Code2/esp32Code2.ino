#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <BitBang_I2C.h>

const char *ssid = "hotspot";
const char *password = "123456789";

Adafruit_AHTX0 aht1 = Adafruit_AHTX0();
Adafruit_AHTX0 aht2 = Adafruit_AHTX0();

TwoWire I2Cone = TwoWire(0); // I2C pe pinii 21(SDA), 22(SCL)
TwoWire I2Ctwo = TwoWire(1); // I2C pe pinii 15(SDA), 2(SCL)

const int powerPin1 = 25; // pin pentru alimentarea senzorului 1
const int powerPin2 = 33; // pin pentru alimentarea senzorului 2

// BitBang I2C for a third sensor (example: SDA=26, SCL=27)
BBI2C bbi2c3;

void setup()
{
  Serial.begin(115200);
  pinMode(powerPin1, OUTPUT);
  pinMode(powerPin2, OUTPUT);
  digitalWrite(powerPin1, LOW); // senzori opriți
  digitalWrite(powerPin2, LOW);

  WiFi.begin(ssid, password);
  Serial.print("Conectare la WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectat!");

  I2Cone.begin(21, 22);
  I2Ctwo.begin(26, 27);

  // Init BitBang I2C (example pins 25, 33)
  bbi2c3.bWire = 0;
  bbi2c3.iSDA = 25;
  bbi2c3.iSCL = 33;
  I2CInit(&bbi2c3, 100000);

  // Initialize AHT20/AHT21 on BitBang bus
  if (initAHT21_BitBang(&bbi2c3))
  {
    Serial.println("AHT21 (BitBang) init OK");
  }
  else
  {
    Serial.println("AHT21 (BitBang) init ERROR");
  }
}

// Helper to initialize and read AHT21/AHT20 via BitBang_I2C
bool initAHT21_BitBang(BBI2C *bus)
{
  uint8_t initCmd[3] = {0xBE, 0x08, 0x00};
  // Send initialization command
  return I2CWrite(bus, 0x38, initCmd, 3) == 0;
}

bool readAHT21_BitBang(BBI2C *bus, float *temp, float *hum)
{
  uint8_t cmdMeasure[3] = {0xAC, 0x38, 0x00};
  uint8_t data[7];
  // Trigger measurement
  if (I2CWrite(bus, 0x30, cmdMeasure, 3) != 0)
    return false;
  delay(250);

  // Wait for busy bit to clear (bit 7 of data[0] == 0)
  for (int i = 0; i < 20; i++)
  {
    if (I2CReadRegister(bus, 0x38, 0x00, data, 7) != 0)
      return false;
    if ((data[0] & 0x80) == 0)
      break;
    delay(20);
  }
  if ((data[0] & 0x80) != 0) // still busy after polling
    return false;

  // Parse data
  uint32_t rawHum = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | (data[3] >> 4);
  uint32_t rawTemp = (((uint32_t)data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];
  *hum = rawHum * 100.0 / 1048576.0;
  *temp = rawTemp * 200.0 / 1048576.0 - 50.0;
  return true;
}

void loop()
{
  sensors_event_t humidity, temperature;
  // Senzor 1 (hardware I2C)
  digitalWrite(powerPin1, HIGH);
  delay(100);
  if (!aht1.begin(&I2Cone))
  {
    Serial.println("Eroare: AHT21 senzor 1 nu detectat!");
  }
  else
  {
    aht1.getEvent(&humidity, &temperature);
    Serial.print("Senzor 1 -> Temp: ");
    Serial.print(temperature.temperature);
    Serial.print(" °C, ");
    Serial.print("Umiditate: ");
    Serial.print(humidity.relative_humidity);
    Serial.println(" %");
  }
  digitalWrite(powerPin1, LOW);
  delay(5000);

  // Senzor 2 (hardware I2C)
  digitalWrite(powerPin2, HIGH);
  delay(100);
  if (!aht2.begin(&I2Ctwo))
  {
    Serial.println("Eroare: AHT21 senzor 2 nu detectat!");
  }
  else
  {
    aht2.getEvent(&humidity, &temperature);
    Serial.print("Senzor 2 -> Temp: ");
    Serial.print(temperature.temperature);
    Serial.print(" °C, ");
    Serial.print("Umiditate: ");
    Serial.print(humidity.relative_humidity);
    Serial.println(" %");
  }
  digitalWrite(powerPin2, LOW);
  delay(5000);

  // Senzor 3 (BitBang I2C)
  float temp3, hum3;
  if (readAHT21_BitBang(&bbi2c3, &temp3, &hum3))
  {
    Serial.print("Senzor 3 (BitBang) -> Temp: ");
    Serial.print(temp3);
    Serial.print(" °C, ");
    Serial.print("Umiditate: ");
    Serial.print(hum3);
    Serial.println(" %");
  }
  else
  {
    Serial.println("Eroare: AHT21 senzor 3 (BitBang) nu detectat!");
  }
  delay(5000);
}
