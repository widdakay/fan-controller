#pragma once
#include "ISensor.hpp"
#include <Wire.h>
#include <Arduino.h>
#include "hal/I2cSwitcher.hpp"
#include "util/Logger.hpp"

namespace hal {

// ZMOD4510 Air Quality Sensor
// NOTE: This is a placeholder implementation as ZMOD4510 may require
// a specific library from Renesas. This provides the interface structure.
class Zmod4510 : public ISensor<app::Zmod4510Reading> {
public:
    explicit Zmod4510(TwoWire& wire, uint8_t addr = 0x32, uint8_t busId = 0)
        : wire_(wire), addr_(addr), busId_(busId), initialized_(false) {}

    bool begin() override {
        hal::I2cSwitcher::instance().useBusId(busId_);
        LOG_DEBUG("[ZMOD4510][bus %u][0x%02X] begin()", busId_, addr_);
        // TODO: Initialize ZMOD4510 according to datasheet
        // This requires specific I2C commands to configure the sensor

        // Check if device responds
        wire_.beginTransmission(addr_);
        if (wire_.endTransmission() != 0) {
            LOG_ERROR("[ZMOD4510][bus %u][0x%02X] begin() FAILED", busId_, addr_);
            return false;
        }

        initialized_ = true;
        return true;
    }

    util::Result<app::Zmod4510Reading, app::SensorError> read() override {
        if (!initialized_) {
            return util::Result<app::Zmod4510Reading, app::SensorError>::Err(
                app::SensorError::NotInitialized);
        }
        hal::I2cSwitcher::instance().useBusId(busId_);
        LOG_DEBUG("[ZMOD4510][bus %u][0x%02X] read() start", busId_, addr_);

        // TODO: Implement actual ZMOD4510 reading protocol
        // The sensor requires specific I2C sequences to:
        // 1. Start measurement
        // 2. Wait for conversion
        // 3. Read results

        // Placeholder implementation
        app::Zmod4510Reading reading;
        reading.tempC = NAN;
        reading.humidity = NAN;
        reading.aqi = NAN;
        reading.ozonePpb = NAN;
        reading.no2Ppb = NAN;
        reading.valid = false;

        LOG_DEBUG("[ZMOD4510][bus %u][0x%02X] read() placeholder", busId_, addr_);
        return util::Result<app::Zmod4510Reading, app::SensorError>::Ok(reading);
    }

    const char* getName() const override {
        return "ZMOD4510";
    }

    bool isConnected() const override {
        if (!initialized_) return false;
        hal::I2cSwitcher::instance().useBusId(busId_);
        wire_.beginTransmission(addr_);
        return (wire_.endTransmission() == 0);
    }

    uint8_t getBusId() const { return busId_; }

private:
    TwoWire& wire_;
    uint8_t addr_;
    uint8_t busId_;
    bool initialized_;
};

} // namespace hal
