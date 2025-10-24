#pragma once
#include "app/Types.hpp"
#include "util/Result.hpp"
#include <INA226_WE.h>
#include <Wire.h>

namespace hal {

class Ina226 {
public:
    explicit Ina226(TwoWire& wire = Wire, uint8_t addr = 0x40)
        : wire_(wire), addr_(addr), ina226_(addr) {}

    bool begin(float shuntResistorOhm = 0.001f) {
        ina226_.init();

        // Set shunt resistor value
        ina226_.setResistorRange(shuntResistorOhm, 3.0);  // 3A max expected current

        // Configure for continuous mode
        ina226_.setMeasureMode(INA226_CONTINUOUS);

        // Configure averaging for noise reduction
        ina226_.setAverage(INA226_AVERAGE_16);

        // Set conversion time (bus and shunt)
        ina226_.setConversionTime(INA226_CONV_TIME_1100, INA226_CONV_TIME_1100);

        return true;
    }

    util::Result<app::PowerReading, app::I2cError> read() {
        app::PowerReading reading;

        reading.busVolts = ina226_.getBusVoltage_V();
        reading.shuntMillivolts = ina226_.getShuntVoltage_mV();
        reading.currentMilliamps = ina226_.getCurrent_mA();
        reading.powerMilliwatts = ina226_.getBusPower();
        reading.loadVolts = reading.busVolts + (reading.shuntMillivolts / 1000.0f);
        reading.overflow = ina226_.overflow;
        reading.valid = true;

        return util::Result<app::PowerReading, app::I2cError>::Ok(reading);
    }

    bool checkOverflow() const {
        return ina226_.overflow;
    }

private:
    TwoWire& wire_;
    uint8_t addr_;
    INA226_WE ina226_;
};

} // namespace hal
