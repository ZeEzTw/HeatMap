#include "ConfigData.h"
#include <esp_system.h>
#include <WiFi.h>
#include "Telegram.h"
#pragma once  // previne includerea de mai multe ori

#define LED_PIN 2
// Global variables for tracking
extern int once;
extern bool resetMessageSent;


void blinkError(int code, int repeat);
void blinkError(int code, int repeat = 1);
void restartESP();
void reconnectWiFi();

void logSystemStatus(unsigned long i2cErrorCount);

