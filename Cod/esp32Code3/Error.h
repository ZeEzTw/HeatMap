#ifndef ERROR_H
#define ERROR_H

#include "ConfigData.h"
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <WiFi.h>
#include "Telegram.h"

// Global variables for error tracking and recovery
extern unsigned long totalI2CErrors;
extern unsigned long influxConsecutiveErrors;
extern unsigned long sensorConsecutiveFailures;

#define LED_PIN 2
// Global variables for tracking
extern int once;
extern bool resetMessageSent;

// Blink the error LED: code = number of blinks per cycle, repeat = number of cycles
void blinkError(int code, int repeat);
// Convenience overload: single-argument wrapper (calls two-arg implementation)
inline void blinkError(int code) { blinkError(code, 1); }
void restartESP();
void reconnectWiFi();

void logSystemStatus(unsigned long i2cErrorCount);

// Helper to convert esp_reset_reason to a readable string
String resetReasonToString(esp_reset_reason_t reason);

#endif // ERROR_H

