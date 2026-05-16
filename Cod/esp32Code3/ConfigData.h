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
const String WIFI_SSID = "Home";
const String WIFI_PASSWORD = "andh!@#45";
/*
to use influxdb localy use this commands in terminal:
# check pc IP
ip a | grep 192.168

# Start InfluxDB
sudo systemctl start influxdb

# Enter CLI
influx

# In CLI create database and check: 
CREATE DATABASE heatmap_eliade
SHOW DATABASES
check if influxdb runs
sudo systemctl status influxdb
and replace the INFLUX_HOST_LOCAL with your pc IP address
and the INFLUX_DB with the database name you created
*/
// InfluxDB v1.8.10 (Local)
const String INFLUX_HOST_LOCAL = "http://192.168.0.160:8086";
const String INFLUX_DB = "heatmap_eliade";
const String INFLUX_USER = "";
const String INFLUX_PASSWORD = "";


//Online InfluxDB v2.x
const String INFLUX_HOST_ONLINE = "https://eu-central-1-1.aws.cloud2.influxdata.com";
const String INFLUX_ORG = "Eli-np";
const String INFLUX_BUCKET = "temperature%20and%20humidity%20data";
const String INFLUX_TOKEN = "j60HrDCjeKlEyAc4m_GrYpFNjIpX--Jv9UX1v7qYtZxdyXfyuPwh_dqLl_bbJCDPi8hk-gJn_dksyh2eE11eug==";

// Sensors
const int NUM_I2C_SENSORS = 3;
const int SDA_PINS[4] = {21, 13, 23};
const int SCL_PINS[4] = {22, 14, 32};
const String SENSOR_IDS[4] = {"105", "206", "105"};

// NTP
const int SYNC_SECOND = 0;

#endif // CONFIG_DATA_H
