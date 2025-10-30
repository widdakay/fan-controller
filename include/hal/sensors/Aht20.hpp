#pragma once
#include "ISensor.hpp"
#include <Arduino.h>
#include "Config.hpp"
#include "hal/I2cSwitcher.hpp"

namespace hal {

// Native AHT20 Temperature & Humidity Sensor implementation
// No external library required - uses direct I2C commands
class Aht20 : public ISensor<app::Si7021Reading> {
public:
    explicit Aht20(TwoWire& wire, uint8_t busId = 0, uint8_t addr = config::I2C_ADDR_AHT20)
        : wire_(wire), busId_(busId), addr_(addr) {}

    bool begin() override {
        hal::I2cSwitcher::instance().useBusId(busId_);
        Serial.printf("[AHT20][bus %u][0x%02X] begin()\n", busId_, addr_);

        // Wait 40ms after power-on
        delay(40);

        // Probe device presence
        wire_.beginTransmission(addr_);
        if (wire_.endTransmission() != 0) {
            Serial.printf("[AHT20][bus %u][0x%02X] probe FAILED\n", busId_, addr_);
            return false;
        }

        // Check status and calibration
        if (!checkStatus_()) {
            Serial.printf("[AHT20][bus %u][0x%02X] status check FAILED\n", busId_, addr_);
            return false;
        }

        // Read status byte to check calibration bit (bit 3)
        uint8_t status = readStatus_();
        if ((status & 0x08) == 0) {
            // Not calibrated - send initialization command
            Serial.printf("[AHT20][bus %u][0x%02X] not calibrated, initializing...\n", busId_, addr_);
            if (!initialize_()) {
                Serial.printf("[AHT20][bus %u][0x%02X] initialization FAILED\n", busId_, addr_);
                return false;
            }
            delay(10);
        }

        Serial.printf("[AHT20][bus %u][0x%02X] initialized successfully\n", busId_, addr_);
        return true;
    }

    util::Result<app::Si7021Reading, app::SensorError> read() override {
        hal::I2cSwitcher::instance().useBusId(busId_);
        Serial.printf("[AHT20][bus %u][0x%02X] read() start\n", busId_, addr_);

        // Trigger measurement: 0xAC 0x33 0x00
        wire_.beginTransmission(addr_);
        wire_.write(0xAC);  // Trigger measurement command
        wire_.write(0x33);  // Parameter byte 1
        wire_.write(0x00);  // Parameter byte 2
        if (wire_.endTransmission() != 0) {
            Serial.printf("[AHT20][bus %u][0x%02X] trigger FAILED\n", busId_, addr_);
            return util::Result<app::Si7021Reading, app::SensorError>::Err(
                app::SensorError::ReadFailed);
        }

        // Wait for measurement to complete (80ms typical)
        delay(80);

        // Wait for busy flag to clear (check status bit 7)
        uint32_t timeout = millis() + 200;
        while (millis() < timeout) {
            uint8_t status = readStatus_();
            if ((status & 0x80) == 0) {
                // Not busy - measurement complete
                break;
            }
            delay(10);
        }

        // Read 6 data bytes
        if (wire_.requestFrom(addr_, (uint8_t)6) != 6) {
            Serial.printf("[AHT20][bus %u][0x%02X] read data FAILED\n", busId_, addr_);
            return util::Result<app::Si7021Reading, app::SensorError>::Err(
                app::SensorError::ReadFailed);
        }

        uint8_t data[6];
        for (int i = 0; i < 6; i++) {
            data[i] = wire_.read();
        }

        // Check if still busy
        if (data[0] & 0x80) {
            Serial.printf("[AHT20][bus %u][0x%02X] still BUSY\n", busId_, addr_);
            return util::Result<app::Si7021Reading, app::SensorError>::Err(
                app::SensorError::ReadFailed);
        }

        // Extract 20-bit humidity value (bits 19:0 of bytes 1-3)
        uint32_t rawHumidity = ((uint32_t)data[1] << 12) |
                               ((uint32_t)data[2] << 4) |
                               ((uint32_t)data[3] >> 4);

        // Extract 20-bit temperature value (bits 19:0 from byte 3-5)
        uint32_t rawTemp = (((uint32_t)data[3] & 0x0F) << 16) |
                           ((uint32_t)data[4] << 8) |
                           (uint32_t)data[5];

        // Convert to physical units
        // Humidity: RH% = (rawValue / 2^20) * 100
        float humidity = (rawHumidity * 100.0f) / 1048576.0f;

        // Temperature: T(°C) = (rawValue / 2^20) * 200 - 50
        float tempC = (rawTemp * 200.0f) / 1048576.0f - 50.0f;

        // Validate readings
        if (!std::isfinite(tempC) || !std::isfinite(humidity)) {
            Serial.printf("[AHT20][bus %u][0x%02X] invalid readings\n", busId_, addr_);
            return util::Result<app::Si7021Reading, app::SensorError>::Err(
                app::SensorError::ReadFailed);
        }

        // Clamp humidity to valid range
        if (humidity > 100.0f) humidity = 100.0f;
        if (humidity < 0.0f) humidity = 0.0f;

        app::Si7021Reading reading;
        reading.tempC = tempC;
        reading.humidity = humidity;
        reading.serialNumber = 0;  // AHT20 doesn't have accessible serial number
        reading.valid = true;

        Serial.printf("[AHT20][bus %u][0x%02X] T=%.2fC RH=%.2f%%\n",
                      busId_, addr_, reading.tempC, reading.humidity);
        return util::Result<app::Si7021Reading, app::SensorError>::Ok(reading);
    }

    std::optional<uint64_t> getSerial() const override {
        return std::nullopt;  // AHT20 doesn't expose serial number
    }

    const char* getName() const override {
        return "AHT20";
    }

    bool isConnected() const override {
        hal::I2cSwitcher::instance().useBusId(busId_);
        wire_.beginTransmission(addr_);
        return (wire_.endTransmission() == 0);
    }

    uint8_t getBusId() const { return busId_; }

private:
    bool checkStatus_() {
        hal::I2cSwitcher::instance().useBusId(busId_);
        wire_.beginTransmission(addr_);
        wire_.write(0x71);  // Status command
        return (wire_.endTransmission() == 0);
    }

    uint8_t readStatus_() {
        hal::I2cSwitcher::instance().useBusId(busId_);
        wire_.beginTransmission(addr_);
        wire_.write(0x71);  // Status command
        wire_.endTransmission();

        if (wire_.requestFrom(addr_, (uint8_t)1) != 1) {
            return 0xFF;  // Error indicator
        }
        return wire_.read();
    }

    bool initialize_() {
        hal::I2cSwitcher::instance().useBusId(busId_);
        wire_.beginTransmission(addr_);
        wire_.write(0xBE);  // Initialize command
        wire_.write(0x08);  // Parameter 1
        wire_.write(0x00);  // Parameter 2
        return (wire_.endTransmission() == 0);
    }

    TwoWire& wire_;
    uint8_t busId_;
    uint8_t addr_;
};

} // namespace hal
