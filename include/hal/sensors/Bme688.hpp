#pragma once
#include "ISensor.hpp"
#include <Adafruit_BME680.h>

namespace hal {

class Bme688 : public ISensor<app::Bme688Reading> {
public:
    explicit Bme688(TwoWire& wire, uint8_t addr = 0x76, uint8_t busId = 0)
        : wire_(wire), addr_(addr), busId_(busId) {}

    bool begin() override {
        if (!bme_.begin(addr_, &wire_)) {
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
        if (!bme_.performReading()) {
            return util::Result<app::Bme688Reading, app::SensorError>::Err(
                app::SensorError::ReadFailed);
        }

        app::Bme688Reading reading;
        reading.tempC = bme_.temperature;
        reading.humidity = bme_.humidity;
        reading.pressurePa = bme_.pressure;
        reading.gasResistance = bme_.gas_resistance;
        reading.valid = true;

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
