#ifndef SENZORS_H
#define SENZORS_Hs

#pragma once
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include "ConfigData.h"
#include <esp_task_wdt.h>

// Global variables for error tracking and recovery
extern unsigned long totalI2CErrors;
extern unsigned long lastI2CRecovery;
extern bool i2cBusLocked;
extern unsigned long influxConsecutiveErrors;
extern unsigned long sensorConsecutiveFailures;



struct SensorData
{
  float temperature;
  float humidity;
};

extern TwoWire I2C;


bool i2cBusRecovery(int scl, int sda);
SensorData readSensorWithRecovery(int sda, int scl, int maxTries = MAX_I2C_RETRIES);
SensorData readSensorWithRecovery(int sda, int scl, int maxTries);

SensorData readSensor(int sda, int scl);


#endif

