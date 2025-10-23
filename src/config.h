#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <driver/ledc.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// —————— USER CONFIGURATION ——————
#define ONE_WIRE_BUS 23
#define TEMPERATURE_PRECISION 12
const int EN_A = 23;
const int EN_B = 22;
const int PWM_A = 19;
const int PWM_B = 18;
const int ISNS_A = 35;
const int ISNS_B = 34;

// LEDC configuration
const ledc_timer_t LEDC_TIMER = LEDC_TIMER_0;
const ledc_mode_t LEDC_MODE = LEDC_HIGH_SPEED_MODE;
const ledc_timer_bit_t LEDC_RESOLUTION = LEDC_TIMER_8_BIT;  // 8-bit (0…255)
const int LEDC_FREQUENCY = 20000;                           // 20 kHz

// We'll drive PWM_A on LEDC channel 0, PWM_B on channel 1:
const ledc_channel_t LEDC_CHANNEL_A = LEDC_CHANNEL_0;
const ledc_channel_t LEDC_CHANNEL_B = LEDC_CHANNEL_1;

// Dead-time and static-friction math
#define PWM_MAX 256
extern float dead_time_us;  // µs of dead-time
extern float drive_volts;
extern float static_friction_volts;

// "power" ∈ [0.0 … 1.0], set via MQTT callback
extern volatile float power;

extern int pwm_a;
extern int pwm_b;

extern float us_period;
extern float dead_ticks;

//— MQTT/Wi-Fi configuration —------------------------------------------------

extern const char* ssid;
extern const char* password;

extern const char* mqtt_server;
extern const int mqtt_port;

extern const char* power_topic;

extern WiFiClient espClient;
extern PubSubClient mqttClient;


extern const char* ca_cert;

extern const char server[];
extern WiFiClientSecure client;
extern OneWire oneWire;
extern DallasTemperature sensors;

extern uint8_t deviceCount;

#endif