#pragma once
#include "util/Result.hpp"
#include "app/Types.hpp"
#include <cstdint>

namespace hal {

// ADC interface for hardware abstraction
class IAdc {
public:
    virtual ~IAdc() = default;

    // Initialize the ADC
    virtual bool begin() = 0;

    // Read voltage from a channel
    virtual util::Result<float, app::I2cError> readVolts(uint8_t channel) = 0;

    // Check if ADC is connected
    virtual bool isConnected() const = 0;

    // Read all channels at once (optional optimization)
    virtual app::AdcReading readAll() {
        app::AdcReading reading;
        auto ch0 = readVolts(0);
        auto ch1 = readVolts(1);
        auto ch2 = readVolts(2);
        auto ch3 = readVolts(3);

        if (ch0.isOk() && ch1.isOk() && ch2.isOk() && ch3.isOk()) {
            reading.motorNtcVolts = ch0.value();
            reading.mcuNtcVolts = ch1.value();
            reading.rail3v3Volts = ch2.value() * 2.0f;  // Account for divider
            reading.rail5vVolts = ch3.value() * 2.0f;
            reading.valid = true;
        }
        return reading;
    }
};

} // namespace hal
