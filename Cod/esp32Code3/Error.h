#include "ConfigData.h"
#include <esp_system.h>
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


void blinkError(int code, int repeat);
void blinkError(int code, int repeat = 1);
void restartESP();
void reconnectWiFi();

void logSystemStatus(unsigned long i2cErrorCount);

