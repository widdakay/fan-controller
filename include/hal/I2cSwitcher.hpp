#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "Config.hpp"

namespace hal {

// Manages a single shared TwoWire instance that can be reconfigured
// at runtime to different SDA/SCL pin pairs. Caches the last pins
// to avoid redundant begin() calls.
class I2cSwitcher {
public:
    static I2cSwitcher& instance() {
        static I2cSwitcher inst;
        return inst;
    }

    // Ensure the shared Wire is configured for the requested pins/frequency
    void use(int sda, int scl, uint32_t frequency = 100000) {
        if (currentSda_ == sda && currentScl_ == scl && currentFreq_ == frequency && initialized_) {
            return;
        }
        // End previous session (safe even if not begun)
        Wire.end();
        Wire.begin(sda, scl, frequency);
        currentSda_ = sda;
        currentScl_ = scl;
        currentFreq_ = frequency;
        initialized_ = true;
    }

    TwoWire& wire() { return Wire; }
    // Convenience: select by logical busId (0 = onboard, 1..n external)
    void useBusId(uint8_t busId, uint32_t frequency = 100000) {
        switch (busId) {
            case 0:
                use(config::PIN_I2C_ONBOARD_SDA, config::PIN_I2C_ONBOARD_SCL, frequency);
                break;
            case 1:
                use(config::PIN_I2C1_SDA, config::PIN_I2C1_SCL, frequency);
                break;
            case 2:
                use(config::PIN_I2C2_SDA, config::PIN_I2C2_SCL, frequency);
                break;
            case 3:
                use(config::PIN_I2C3_SDA, config::PIN_I2C3_SCL, frequency);
                break;
            default:
                use(config::PIN_I2C_ONBOARD_SDA, config::PIN_I2C_ONBOARD_SCL, frequency);
                break;
        }
    }


    int currentSda() const { return currentSda_; }
    int currentScl() const { return currentScl_; }

private:
    I2cSwitcher() = default;

    int currentSda_ = -1;
    int currentScl_ = -1;
    uint32_t currentFreq_ = 0;
    bool initialized_ = false;
};

} // namespace hal


