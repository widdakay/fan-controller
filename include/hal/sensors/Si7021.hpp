#pragma once
#include "ISensor.hpp"
#include <Adafruit_Si7021.h>

namespace hal {

class Si7021 : public ISensor<app::Si7021Reading> {
public:
    explicit Si7021(TwoWire& wire, uint8_t busId = 0)
        : wire_(wire), busId_(busId) {}

    bool begin() override {
        if (!sensor_.begin()) {
            return false;
        }

        // Note: readSerialNumber() is void, serial read must be done differently
        // For now, serialNumber will be 0
        serialNumber_ = 0;

        return true;
    }

    util::Result<app::Si7021Reading, app::SensorError> read() override {
        float temp = sensor_.readTemperature();
        float humidity = sensor_.readHumidity();

        // Check for invalid readings (sensor typically returns NAN on error)
        if (!std::isfinite(temp) || !std::isfinite(humidity)) {
            return util::Result<app::Si7021Reading, app::SensorError>::Err(
                app::SensorError::ReadFailed);
        }

        app::Si7021Reading reading;
        reading.tempC = temp;
        reading.humidity = humidity;
        reading.serialNumber = serialNumber_;
        reading.valid = true;

        return util::Result<app::Si7021Reading, app::SensorError>::Ok(reading);
    }

    std::optional<uint64_t> getSerial() const override {
        return serialNumber_;
    }

    const char* getName() const override {
        return "Si7021";
    }

    bool isConnected() const override {
        return true;  // Library doesn't provide easy check
    }

    uint8_t getBusId() const { return busId_; }

private:
    TwoWire& wire_;
    uint8_t busId_;
    uint64_t serialNumber_ = 0;
    Adafruit_Si7021 sensor_;
};

} // namespace hal
