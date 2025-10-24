#pragma once
#include "IAdc.hpp"
#include <Adafruit_ADS1X15.h>
#include <Wire.h>

namespace hal {

class Ads1115 : public IAdc {
public:
    explicit Ads1115(TwoWire& wire = Wire, uint8_t addr = 0x48)
        : wire_(wire), addr_(addr), adc_() {}

    bool begin() override {
        if (!adc_.begin(addr_, &wire_)) {
            return false;
        }
        // Set gain to measure up to 4.096V (default)
        adc_.setGain(GAIN_ONE);
        return true;
    }

    util::Result<float, app::I2cError> readVolts(uint8_t channel) override {
        if (channel > 3) {
            return util::Result<float, app::I2cError>::Err(app::I2cError::Unknown);
        }

        int16_t raw = adc_.readADC_SingleEnded(channel);
        // ADS1115 is 16-bit, gain of 1 = ±4.096V range
        // LSB = 4.096V / 32768 = 0.000125V
        float volts = raw * 0.000125f;

        if (volts < 0.0f || volts > 5.0f) {
            return util::Result<float, app::I2cError>::Err(app::I2cError::InvalidData);
        }

        return util::Result<float, app::I2cError>::Ok(volts);
    }

private:
    TwoWire& wire_;
    uint8_t addr_;
    Adafruit_ADS1115 adc_;
};

} // namespace hal
