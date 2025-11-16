#pragma once
#include "app/Types.hpp"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <vector>

namespace hal {

class OneWireBus {
public:
    explicit OneWireBus(uint8_t pin, uint8_t busId)
        : pin_(pin), busId_(busId), oneWire_(pin), sensors_(&oneWire_) {}

    bool begin() {
        sensors_.begin();
        deviceCount_ = sensors_.getDeviceCount();

        Serial.printf("OneWire bus %d: Found %d devices\n", busId_, deviceCount_);

        // Set resolution to 12-bit for accuracy
        sensors_.setResolution(12);

        return deviceCount_ > 0;
    }

    void requestTemperatures() {
        sensors_.requestTemperatures();
    }

    std::vector<app::OneWireReading> readAll() {
        std::vector<app::OneWireReading> readings;

        for (uint8_t i = 0; i < deviceCount_; i++) {
            DeviceAddress addr;
            if (!sensors_.getAddress(addr, i)) {
                continue;
            }

            float tempC = sensors_.getTempC(addr);

            // Convert address to uint64_t (same logic as begin())
            uint64_t addr64 = 0;
            for (int j = 0; j < 8; j++) {
                addr64 = (addr64 << 8) | addr[j];
            }

            app::OneWireReading reading;
            reading.busId = busId_;
            reading.address = addr64;
            reading.tempC = tempC;
            reading.valid = (tempC != DEVICE_DISCONNECTED_C) &&
                           (tempC > -40.0f) && (tempC < 125.0f);

            readings.push_back(reading);
        }

        return readings;
    }

    uint8_t getDeviceCount() const { return deviceCount_; }
    uint8_t getBusId() const { return busId_; }

private:
    uint8_t pin_;
    uint8_t busId_;
    OneWire oneWire_;
    DallasTemperature sensors_;
    uint8_t deviceCount_ = 0;
};

} // namespace hal
