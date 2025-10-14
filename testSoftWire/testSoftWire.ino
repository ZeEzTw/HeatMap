#include <BitBangAHT21ELI.h>
BitBangAHT21ELI aht21(26, 25);
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n\n=== AHT21 with Bit-Banged I2C ===");
}

void loop() {
  float temp, hum;
  aht21.aht_get_data(&temp, &hum);
  Serial.print("Temp: ");
  Serial.print(temp, 2);
  Serial.print(" °C  |  Humidity: ");
  Serial.print(hum, 2);
  Serial.println(" %");
  delay(2000);
}