#pragma once
#include "ISensor.hpp"
#include <Adafruit_BME680.h>
#include <Arduino.h>
#include "hal/I2cSwitcher.hpp"
#include "util/Logger.hpp"

namespace hal {

class Bme688 : public ISensor<app::Bme688Reading> {
public:
    explicit Bme688(TwoWire& wire, uint8_t addr = 0x76, uint8_t busId = 0)
        : wire_(wire), addr_(addr), busId_(busId) {}

    bool begin() override {
        hal::I2cSwitcher::instance().useBusId(busId_);
        Logger::debug("[BME688][bus %u][0x%02X] begin()", busId_, addr_);
        if (!bme_.begin(addr_, &wire_)) {
            Logger::error("[BME688][bus %u][0x%02X] begin() FAILED", busId_, addr_);
            return false;
        }

        // Set up oversampling and filter
        bme_.setTemperatureOversampling(BME680_OS_8X);
        bme_.setHumidityOversampling(BME680_OS_2X);
        bme_.setPressureOversampling(BME680_OS_4X);
        bme_.setIIRFilterSize(BME680_FILTER_SIZE_3);
        bme_.setGasHeater(320, 150);  // 320°C for 150 ms

        return true;
    }

    util::Result<app::Bme688Reading, app::SensorError> read() override {
        hal::I2cSwitcher::instance().useBusId(busId_);
        Logger::debug("[BME688][bus %u][0x%02X] read() start", busId_, addr_);
        if (!bme_.performReading()) {
            Logger::error("[BME688][bus %u][0x%02X] read() FAILED", busId_, addr_);
            return util::Result<app::Bme688Reading, app::SensorError>::Err(
                app::SensorError::ReadFailed);
        }

        app::Bme688Reading reading;
        reading.tempC = bme_.temperature;
        reading.humidity = bme_.humidity;
        reading.pressurePa = bme_.pressure;
        reading.gasResistance = bme_.gas_resistance;
        reading.valid = true;

        Logger::debug("[BME688][bus %u][0x%02X] T=%.2fC RH=%.2f%% P=%.0fPa Gas=%.0f",
                      busId_, addr_, reading.tempC, reading.humidity,
                      reading.pressurePa, reading.gasResistance);
        return util::Result<app::Bme688Reading, app::SensorError>::Ok(reading);
    }

    const char* getName() const override {
        return "BME688";
    }

    bool isConnected() const override {
        // Try to read chip ID
        return true;  // BME680 library doesn't expose easy connection check
    }

    uint8_t getBusId() const { return busId_; }

private:
    TwoWire& wire_;
    uint8_t addr_;
    uint8_t busId_;
    Adafruit_BME680 bme_;
};

} // namespace hal
