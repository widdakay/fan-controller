#ifndef MQTT_H
#define MQTT_H

#include "config.h"

// Function declarations
void callback(char* topic, byte* payload, unsigned int length);
void reconnectMqtt();

#endif