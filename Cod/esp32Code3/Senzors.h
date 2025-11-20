#ifndef SENZORS_H
#define SENZORS_H

#pragma once
#include "ConfigData.h"
#include <esp_task_wdt.h>

// Global variables for error tracking and recovery
extern unsigned long totalI2CErrors;
extern unsigned long influxConsecutiveErrors;
extern unsigned long sensorConsecutiveFailures;

#endif

