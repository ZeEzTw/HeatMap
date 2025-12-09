#ifndef CONFIG_DATA_H
#define CONFIG_DATA_H

#include <Arduino.h>
//for TelegramBot
const String botToken = "8433695203:AAGs6I-c5zyHJFurlDWAW6m8qniScs7Uq-g";
const String chatID = "5291138966";


// All configuration data in one place for easy editing
const int WDT_TIMEOUT = 120;  // seconds - increased timeout
const int sensorDelayMs = 100; // increased for long wires and stability
const int cycleDelayMs = 3000; // increased cycle delay to prevent I2C conflicts
const int MAX_I2C_RETRIES = 3;
const int MAX_SENSOR_FAILURES = 5;
const int I2C_RECOVERY_DELAY = 100;
const int SENSOR_INIT_DELAY = 50;
// WiFi
const String WIFI_SSID = "GuestELI";
const String WIFI_PASSWORD = "guestELI18";

// InfluxDB v1.8.10 (Local)
const String INFLUX_HOST_LOCAL = "http://192.168.5.185:8086";
const String INFLUX_DB = "temperatura_humidity_data";
const String INFLUX_USER = "";  // Leave empty if no auth
const String INFLUX_PASSWORD = "";  // Leave empty if no auth


//Online InfluxDB v2.x
const String INFLUX_HOST_ONLINE = "https://eu-central-1-1.aws.cloud2.influxdata.com";
const String INFLUX_ORG = "Eli-np";
const String INFLUX_BUCKET = "temperature%20and%20humidity%20data";
const String INFLUX_TOKEN = "j60HrDCjeKlEyAc4m_GrYpFNjIpX--Jv9UX1v7qYtZxdyXfyuPwh_dqLl_bbJCDPi8hk-gJn_dksyh2eE11eug==";

// Sensors
const int NUM_I2C_SENSORS = 3;
const int SDA_PINS[4] = {16, 5, 26, 14};
const int SCL_PINS[4] = {4, 17, 25, 27};
const String SENSOR_IDS[4] = {"008", "009", "010", "004"};

// NTP
const int SYNC_SECOND = 0;

#endif // CONFIG_DATA_H
