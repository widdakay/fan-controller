#pragma once
#include <cstdint>
#include <cmath>
#include <string>
#include <array>

namespace app {

// ============================================================================
// Error Types
// ============================================================================

enum class I2cError {
    NotFound,
    Timeout,
    Nack,
    BusError,
    InvalidData,
    Unknown
};

enum class SensorError {
    NotInitialized,
    ReadFailed,
    InvalidData,
    Timeout
};

enum class WiFiError {
    NoCredentials,
    ScanFailed,
    ConnectionFailed,
    Timeout,
    Unknown
};

enum class HttpError {
    ConnectionFailed,
    RequestFailed,
    InvalidResponse,
    Timeout
};

enum class MqttError {
    NotConnected,
    ConnectionFailed,
    PublishFailed,
    SubscribeFailed
};

// ============================================================================
// Hardware Reading Types
// ============================================================================

struct AdcReading {
    float motorNtcVolts = NAN;
    float mcuNtcVolts = NAN;
    float rail3v3Volts = NAN;
    float rail5vVolts = NAN;
    bool valid = false;
};

struct ThermistorReading {
    float tempC = NAN;
    float resistance = NAN;
    float voltage = NAN;
    bool inRange = false;
};

struct PowerReading {
    float busVolts = NAN;
    float shuntMillivolts = NAN;
    float currentMilliamps = NAN;
    float powerMilliwatts = NAN;
    float loadVolts = NAN;
    bool overflow = false;
    bool valid = false;
};

struct MotorStatus {
    float dutyCycle = 0.0f;      // 0.0 - 1.0
    bool directionForward = true;
    bool enAEnabled = false;
    bool enBEnabled = false;
    bool fault = false;
};

// ============================================================================
// Environmental Sensor Readings
// ============================================================================

struct Bme688Reading {
    float tempC = NAN;
    float humidity = NAN;
    float pressurePa = NAN;
    float gasResistance = NAN;
    bool valid = false;
};

struct Si7021Reading {
    float tempC = NAN;
    float humidity = NAN;
    uint64_t serialNumber = 0;
    bool valid = false;
};

struct Zmod4510Reading {
    float tempC = NAN;
    float humidity = NAN;
    float aqi = NAN;              // Air Quality Index
    float ozonePpb = NAN;         // Ozone (ppb)
    float no2Ppb = NAN;           // Nitrogen Dioxide (ppb)
    bool valid = false;
};

struct OneWireReading {
    uint8_t busId;                // Which bus (0-3)
    uint64_t address;             // 64-bit OneWire address
    float tempC = NAN;
    bool valid = false;
};

// ============================================================================
// System Health Data
// ============================================================================

struct HealthData {
    // Timestamps
    uint32_t uptimeMs = 0;

    // Temperatures
    ThermistorReading motorTemp;
    ThermistorReading mcuExternalTemp;
    float mcuInternalTempC = NAN;

    // Power supply
    float rail3v3 = NAN;
    float rail5v = NAN;
    PowerReading inputPower;

    // Motor status
    MotorStatus motor;

    // System info
    uint32_t freeHeap = 0;
    int8_t wifiRssi = 0;
    bool mqttConnected = false;
};

// ============================================================================
// Boot Information
// ============================================================================

struct BootInfo {
    uint64_t chipId = 0;
    const char* resetReason = "";
    uint32_t sketchSize = 0;
    uint32_t freeSketchSpace = 0;
    uint32_t heapSize = 0;
    const char* firmwareVersion = "";
};

// ============================================================================
// Network Information
// ============================================================================

struct WiFiScanResult {
    std::string ssid;
    int8_t rssi;
    uint8_t channel;
    bool encrypted;
    std::array<uint8_t, 6> bssid;    // Access point MAC address
};

} // namespace app
