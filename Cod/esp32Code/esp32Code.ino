#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>

const char* ssid = "hotspot";
const char* password = "123456789";

// InfluxDB Cloud config
const char* influx_host = "https://eu-central-1-1.aws.cloud2.influxdata.com";
const char* org = "Eli-np";
const char* bucket = "temperature%20and%20humidity%20data";  // %20 în loc de spațiu!
const char* token = "j60HrDCjeKlEyAc4m_GrYpFNjIpX--Jv9UX1v7qYtZxdyXfyuPwh_dqLl_bbJCDPi8hk-gJn_dksyh2eE11eug==";

Adafruit_AHTX0 aht;

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.print("Conectare la WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectat!");

  Wire.begin(21, 22);
  if (!aht.begin()) {
    Serial.println("Eroare: AHT21 nu detectat!");
    while (1) delay(10);
  }
  Serial.println("Senzor AHT21 OK");
}
void loop() {
  sensors_event_t humidity, temperature;
  aht.getEvent(&humidity, &temperature);

  float temp = temperature.temperature;
  float hum = humidity.relative_humidity;

  if (isnan(temp) || isnan(hum)) {
    Serial.println("Date senzor invalide.");
    delay(10000);
    return;
  }

  Serial.print("Temp: "); Serial.print(temp); Serial.print(" °C | ");
  Serial.print("Umiditate: "); Serial.print(hum); Serial.println(" %");

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String url = String(influx_host) + "/api/v2/write?org=" + org + "&bucket=" + bucket + "&precision=s";
    http.begin(url);
    http.addHeader("Authorization", "Token " + String(token));
    http.addHeader("Content-Type", "text/plain");

    // Trimite batch: doua linii separate prin '\n'
    String body = "";
    body += "temperature,locatie=birou value=" + String(temp, 2) + "\n";
    body += "humidity,locatie=birou value=" + String(hum, 2);

    Serial.println("Trimis catre InfluxDB:");
    Serial.println(body);

    int status = http.POST(body);
    Serial.print("Status POST: ");
    Serial.println(status);
    http.end();
  }

  delay(10000);
}
