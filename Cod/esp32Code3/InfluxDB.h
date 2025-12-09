#ifndef INFLUXDB_H
#define INFLUXDB_H

#include "ConfigData.h"
#include "Error.h"
#include <HTTPClient.h>
#include <WiFi.h>

// InfluxDB v1.8.10 write function
void writeToInfluxDBLocal(const String &body);
void writeToInfluxDBOnline(const String &body);
#endif // INFLUXDB_H
