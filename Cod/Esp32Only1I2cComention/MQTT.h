#ifndef MQTT_H
#define MQTT_H

#include <PubSubClient.h>
#include <Arduino.h>
#include <WiFi.h>

// NTP config
extern const char* ntpServer;
extern const long gmtOffset_sec;
extern const int daylightOffset_sec;

#define LED_PIN 2

extern String deviceID;
extern PubSubClient mqttClient;

extern bool blinking;
extern unsigned long blinkStartTime;

void blinkLED();
void reconnectMQTT();
void callback(char* topic, byte* payload, unsigned int length);

#endif
