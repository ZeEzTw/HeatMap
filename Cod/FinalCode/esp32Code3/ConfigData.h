#ifndef CONFIG_DATA_H
#define CONFIG_DATA_H

#include <Arduino.h>

// All configuration data in one place for easy editing

// WiFi
const String WIFI_SSID = "Guests";
const String WIFI_PASSWORD = "guestELI18";

// InfluxDB
const String INFLUX_HOST = "https://eu-central-1-1.aws.cloud2.influxdata.com";
const String INFLUX_ORG = "Eli-np";
const String INFLUX_BUCKET = "temperature%20and%20humidity%20data";
const String INFLUX_TOKEN = "j60HrDCjeKlEyAc4m_GrYpFNjIpX--Jv9UX1v7qYtZxdyXfyuPwh_dqLl_bbJCDPi8hk-gJn_dksyh2eE11eug==";

// Sensors
const int NUM_I2C_SENSORS = 4;
const int SDA_PINS[4] = {16, 5, 26, 14};
const int SCL_PINS[4] = {4, 17, 25, 27};
const String SENSOR_IDS[4] = {"001", "002", "003", "004"};

// Delays
const int SENSOR_DELAY_MS = 400;
const int CYCLE_DELAY_MS = 10000;

// NTP
const int SYNC_SECOND = 0;

#endif // CONFIG_DATA_H
