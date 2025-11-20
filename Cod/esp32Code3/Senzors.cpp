#include "Senzors.h"

// Global error tracking variables
unsigned long totalI2CErrors = 0;
unsigned long influxConsecutiveErrors = 0;
unsigned long sensorConsecutiveFailures = 0;