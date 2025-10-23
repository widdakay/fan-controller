#ifndef PWM_H
#define PWM_H

#include "config.h"
#include <driver/ledc.h>

// Function declarations
void setupLEDC();
void writeDuty(ledc_channel_t channel, int duty);
void set_vals();

#endif