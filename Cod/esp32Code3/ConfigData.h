#ifndef CONFIG_DATA_H
#define CONFIG_DATA_H

#include <Arduino.h>
//for TelegramBot
const String botToken = "8433695203:AAGs6I-c5zyHJFurlDWAW6m8qniScs7Uq-g";
const String chatID = "5291138966";


// All configuration data in one place for easy editing
const int WDT_TIMEOUT = 120;  // seconds - increased timeout
const int sensorDelayMs = 100; // increased for long wires and stability
const int cycleDelayMs = 1000; // increased cycle delay to prevent I2C conflicts
const int MAX_I2C_RETRIES = 3;
const int MAX_SENSOR_FAILURES = 5;
const int I2C_RECOVERY_DELAY = 100;
const int SENSOR_INIT_DELAY = 50;
// WiFi
const String WIFI_SSID = "GuestELI";
const String WIFI_PASSWORD = "guestELI18";

// InfluxDB
const String INFLUX_HOST = "https://eu-central-1-1.aws.cloud2.influxdata.com";
const String INFLUX_ORG = "Eli-np";
const String INFLUX_BUCKET = "temperature%20and%20humidity%20data";
const String INFLUX_TOKEN = "j60HrDCjeKlEyAc4m_GrYpFNjIpX--Jv9UX1v7qYtZxdyXfyuPwh_dqLl_bbJCDPi8hk-gJn_dksyh2eE11eug==";

// Sensors
const int NUM_I2C_SENSORS = 1;
const int SDA_PINS[4] = {26, 5, 26, 14};
const int SCL_PINS[4] = {25, 17, 25, 27};
const String SENSOR_IDS[4] = {"001", "002", "003", "004"};

// NTP
const int SYNC_SECOND = 0;

#endif // CONFIG_DATA_H