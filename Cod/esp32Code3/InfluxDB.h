#ifndef INFLUXDB_H
#define INFLUXDB_H

#include "ConfigData.h"
#include "Error.h"
#include <HTTPClient.h>
#include <WiFi.h>

// Enhanced InfluxDB writing with comprehensive error handling
void writeToInfluxDB(const String &body);

#endif // INFLUXDB_H
