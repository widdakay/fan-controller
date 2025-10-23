#ifndef SENSORS_H
#define SENSORS_H

#include "config.h"

// Function declarations
String addressToString(const DeviceAddress deviceAddress);
void setupSensors();
String getTemperatureData();

#endif