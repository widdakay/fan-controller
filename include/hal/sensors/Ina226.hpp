#pragma once
#include "app/Types.hpp"
#include "util/Result.hpp"
#include "hal/sensors/ISensor.hpp"
#include <INA226_WE.h>
#include <Wire.h>
#include <Arduino.h>
#include "hal/I2cSwitcher.hpp"

namespace hal {

class Ina226 : public ISensor<app::PowerReading> {
public:
    explicit Ina226(TwoWire& wire = Wire, uint8_t addr = 0x40, uint8_t busId = 0)
        : wire_(wire), addr_(addr), busId_(busId), ina226_(addr) {}

    bool begin() override { return begin(0.001f); }

    bool begin(float shuntResistorOhm) {
        hal::I2cSwitcher::instance().useBusId(busId_);
        Serial.printf("[INA226][bus %u][0x%02X] begin()\n", busId_, addr_);
        if (!ina226_.init()) {
            Serial.printf("[INA226][bus %u][0x%02X] begin() FAILED\n", busId_, addr_);
            return false;
        }

        // Set shunt resistor value
        ina226_.setResistorRange(shuntResistorOhm);  // 30A max expected current

        // Configure for continuous mode
        ina226_.setMeasureMode(INA226_CONTINUOUS);

        // Configure averaging for noise reduction
        ina226_.setAverage(INA226_AVERAGE_512);

        // Set conversion time (bus and shunt)
        ina226_.setConversionTime(INA226_CONV_TIME_1100, INA226_CONV_TIME_1100);

        return true;
    }

    util::Result<app::PowerReading, app::SensorError> read() override {
        hal::I2cSwitcher::instance().useBusId(busId_);
        Serial.printf("[INA226][bus %u][0x%02X] read() start\n", busId_, addr_);
        app::PowerReading reading;

        reading.busVolts = ina226_.getBusVoltage_V();
        reading.shuntMillivolts = ina226_.getShuntVoltage_mV();
        reading.currentMilliamps = ina226_.getCurrent_mA();
        reading.powerMilliwatts = ina226_.getBusPower();
        reading.loadVolts = reading.busVolts + (reading.shuntMillivolts / 1000.0f);
        reading.overflow = ina226_.overflow;
        reading.valid = true;

        Serial.printf("[INA226][bus %u][0x%02X] V=%.3fV I=%.1fmA P=%.1fmW\n",
                      busId_, addr_, reading.busVolts, reading.currentMilliamps, reading.powerMilliwatts);
        return util::Result<app::PowerReading, app::SensorError>::Ok(reading);
    }

    const char* getName() const override { return "INA226"; }

    bool isConnected() const override {
        // Could implement by trying to read a register
        return true; // Assume connected if begin() succeeded
    }

    bool checkOverflow() const {
        return ina226_.overflow;
    }

private:
    TwoWire& wire_;
    uint8_t addr_;
    uint8_t busId_ = 0;
    INA226_WE ina226_;
};

} // namespace hal
