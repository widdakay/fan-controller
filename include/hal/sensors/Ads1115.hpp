#pragma once
#include "hal/sensors/IAdc.hpp"
#include <Adafruit_ADS1X15.h>
#include <Wire.h>
#include <Arduino.h>
#include "hal/I2cSwitcher.hpp"

namespace hal {

class Ads1115 : public IAdc {
public:
    explicit Ads1115(TwoWire& wire = Wire, uint8_t addr = 0x48, uint8_t busId = 0)
        : wire_(wire), addr_(addr), busId_(busId), adc_() {}

    bool begin() override {
        hal::I2cSwitcher::instance().useBusId(busId_);
        Serial.printf("[ADS1115][bus %u][0x%02X] begin()\n", busId_, addr_);
        if (!adc_.begin(addr_, &wire_)) {
            Serial.printf("[ADS1115][bus %u][0x%02X] begin() FAILED\n", busId_, addr_);
            return false;
        }
        // Set gain to measure up to 4.096V (default)
        adc_.setGain(GAIN_ONE);
        return true;
    }

    util::Result<float, app::I2cError> readVolts(uint8_t channel) override {
        hal::I2cSwitcher::instance().useBusId(busId_);
        Serial.printf("[ADS1115][bus %u][0x%02X] read(ch=%u) start\n", busId_, addr_, channel);
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

        Serial.printf("[ADS1115][bus %u][0x%02X] ch%u = %.4f V\n", busId_, addr_, channel, volts);
        return util::Result<float, app::I2cError>::Ok(volts);
    }

    bool isConnected() const override {
        // Assume connected if begin() succeeded
        return true;
    }

private:
    TwoWire& wire_;
    uint8_t addr_;
    uint8_t busId_ = 0;
    Adafruit_ADS1115 adc_;
};

} // namespace hal
