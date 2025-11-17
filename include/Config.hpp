#pragma once
#include <cstdint>
#include <array>
#include <utility>
#include "SecureConfig.hpp"

namespace config {

// ============================================================================
// Pin Definitions
// ============================================================================

// Debug LEDs
inline constexpr int PIN_LED_GREEN  = 4;
inline constexpr int PIN_LED_ORANGE = 5;
inline constexpr int PIN_LED_RED    = 6;
inline constexpr int PIN_LED_BLUE   = 7;

// OneWire buses
inline constexpr int PIN_ONEWIRE_1 = 3;
inline constexpr int PIN_ONEWIRE_2 = 46;
inline constexpr int PIN_ONEWIRE_3 = 9;
inline constexpr int PIN_ONEWIRE_4 = 10;

// External I2C buses
inline constexpr int PIN_I2C1_SDA = 11;
inline constexpr int PIN_I2C1_SCL = 12;
inline constexpr int PIN_I2C2_SDA = 13;
inline constexpr int PIN_I2C2_SCL = 14;
inline constexpr int PIN_I2C3_SDA = 21;
inline constexpr int PIN_I2C3_SCL = 47;
inline constexpr int PIN_I2C4_SDA = 48;
inline constexpr int PIN_I2C4_SCL = 45;

// Onboard I2C
inline constexpr int PIN_I2C_ONBOARD_SDA = 1;
inline constexpr int PIN_I2C_ONBOARD_SCL = 2;

// I2C Bus ID to Pin Mapping
// Returns {sda, scl} for a given bus ID (0 = onboard, 1-4 = external buses)
// Returns {-1, -1} for invalid bus IDs
constexpr std::pair<int, int> getI2CPins(uint8_t busId) {
    switch (busId) {
        case 0:
            return {PIN_I2C_ONBOARD_SDA, PIN_I2C_ONBOARD_SCL};
        case 1:
            return {PIN_I2C1_SDA, PIN_I2C1_SCL};
        case 2:
            return {PIN_I2C2_SDA, PIN_I2C2_SCL};
        case 3:
            return {PIN_I2C3_SDA, PIN_I2C3_SCL};
        case 4:
            return {PIN_I2C4_SDA, PIN_I2C4_SCL};
        default:
            return {-1, -1};
    }
}

// Motor controller
inline constexpr int PIN_MOTOR_IN_A = 41;
inline constexpr int PIN_MOTOR_IN_B = 35;
inline constexpr int PIN_MOTOR_EN_A = 40;
inline constexpr int PIN_MOTOR_EN_B = 36;
inline constexpr int PIN_MOTOR_PWM  = 38;

// Debug Serial (already defined by framework)
// TXD0, RXD0

// ============================================================================
// I2C Addresses
// ============================================================================

inline constexpr uint8_t I2C_ADDR_ADS1115 = 0x48;
inline constexpr uint8_t I2C_ADDR_INA226  = 0x40;
inline constexpr uint8_t I2C_ADDR_BME688  = 0x76;  // or 0x77
inline constexpr uint8_t I2C_ADDR_AHT20   = 0x38;  // AHT20 temperature & humidity sensor
inline constexpr uint8_t I2C_ADDR_ZMOD4510 = 0x32;

// ============================================================================
// Hardware Configuration
// ============================================================================

// ADS1115 ADC
enum class AdcChannel : uint8_t {
    MotorNTC = 0,  // AIN0: Motor thermistor
    McuNTC   = 1,  // AIN1: MCU/board thermistor
    Rail3V3  = 2,  // AIN2: 3.3V rail (via 2:1 divider)
    Rail5V   = 3   // AIN3: 5V rail (via 2:1 divider)
};

// INA226 Power Monitor
inline constexpr float INA226_SHUNT_OHM = 0.001f;  // 1 milliohm

// Motor PWM
inline constexpr uint32_t MOTOR_PWM_FREQ_HZ = 20000;  // 20 kHz
inline constexpr uint8_t  MOTOR_PWM_BITS    = 10;     // 10-bit resolution
inline constexpr uint32_t MOTOR_DIRECTION_DEADTIME_MS = 2;

// Thermistor parameters (10k NTC)
inline constexpr float THERMISTOR_R0 = 10000.0f;  // 10k ohm at 25°C
inline constexpr float THERMISTOR_SERIES_R = 10000.0f;  // Series resistor

// ============================================================================
// Timing Configuration
// ============================================================================

inline constexpr uint32_t WATCHDOG_TIMEOUT_MS = 60000;  // 60 seconds (allow for WiFi scan during setup)

inline constexpr uint32_t TASK_HEALTH_REPORT_MS = 5000;   // 5 seconds
inline constexpr uint32_t TASK_MQTT_PUBLISH_MS = 10000;   // 10 seconds
inline constexpr uint32_t TASK_FW_CHECK_MS = 3600000;     // 1 hour
inline constexpr uint32_t TASK_LED_HEARTBEAT_MS = 1000;   // 1 second
inline constexpr uint32_t TASK_SENSOR_READ_MS = 5000;     // 5 seconds

inline constexpr uint32_t LED_ERROR_FLASH_MS = 500;
inline constexpr uint32_t LED_HEARTBEAT_MS = 100;

inline constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 30000;  // 30 seconds
inline constexpr uint32_t MQTT_RECONNECT_INTERVAL_MS = 5000;

// OneWire temperature request needs at least 750ms for conversion
inline constexpr uint32_t ONEWIRE_CONVERSION_MS = 800;

// ============================================================================
// Network Configuration
// ============================================================================

// WiFi credentials and MQTT server are defined in SecureConfig.hpp
// Copy SecureConfig.hpp.example to SecureConfig.hpp and update with your credentials

// HTTPS API Endpoints
inline constexpr const char* API_INFLUXDB = "https://data.yoerik.com/particle/log";
inline constexpr const char* API_FW_UPDATE = "https://data.yoerik.com/particle/fw/update";

// // MQTT Configuration
// inline constexpr uint16_t MQTT_PORT = 1883;
// inline constexpr const char* MQTT_TOPIC_POWER_COMMAND = "home/fan1/power";
// inline constexpr const char* MQTT_TOPIC_POWER_STATUS = "home/fan1/power/status";

// // Device identification
// inline constexpr const char* DEVICE_NAME = "UnderHouseFan";

// // MQTT Configuration
// inline constexpr uint16_t MQTT_PORT = 1883;
// inline constexpr const char* MQTT_TOPIC_POWER_COMMAND = "lucect/fan1/power";
// inline constexpr const char* MQTT_TOPIC_POWER_STATUS = "lucect/fan1/power/status";

// // Device identification
// inline constexpr const char* DEVICE_NAME = "LuceCTTestFan";


// // MQTT Configuration
// inline constexpr uint16_t MQTT_PORT = 1883;
// inline constexpr const char* MQTT_TOPIC_POWER_COMMAND = "testboard/fan1/power";
// inline constexpr const char* MQTT_TOPIC_POWER_STATUS = "testboard/fan1/power/status";

// // Device identification
// inline constexpr const char* DEVICE_NAME = "TestBoard";


// MQTT Configuration
inline constexpr uint16_t MQTT_PORT = 1883;
inline constexpr const char* MQTT_TOPIC_POWER_COMMAND = "testboard3/fan1/power";
inline constexpr const char* MQTT_TOPIC_POWER_STATUS = "testboard3/fan1/power/status";

// Device identification
inline constexpr const char* DEVICE_NAME = "TestBoard3";

// ============================================================================
// Compile-time validation
// ============================================================================

// Ensure pins are in valid range for ESP32-S3
constexpr bool isValidGPIO(int pin) {
    return pin >= 0 && pin < 50;
}

static_assert(isValidGPIO(PIN_LED_GREEN), "Invalid LED pin");
static_assert(isValidGPIO(PIN_MOTOR_PWM), "Invalid motor PWM pin");
static_assert(MOTOR_PWM_FREQ_HZ > 0 && MOTOR_PWM_FREQ_HZ <= 40000, "Invalid PWM frequency");

} // namespace config
