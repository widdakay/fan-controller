#pragma once
#include <Wire.h>
#include <memory>
#include <vector>
#include "hal/I2cSwitcher.hpp"

namespace hal {

// I2C bus wrapper with scanning and recovery capabilities
class I2cBus {
public:
    explicit I2cBus(int sda, int scl, uint8_t busId)
        : sda_(sda), scl_(scl), busId_(busId) {}

    bool begin(uint32_t frequency = 100000) {
        hal::I2cSwitcher::instance().use(sda_, scl_, frequency);
        return true;
    }

    // Scan for I2C devices on this bus
    std::vector<uint8_t> scan() {
        std::vector<uint8_t> devices;

        // Ensure the bus is selected before scanning
        hal::I2cSwitcher::instance().use(sda_, scl_);
        for (uint8_t addr = 1; addr < 127; addr++) {
            hal::I2cSwitcher::instance().wire().beginTransmission(addr);
            if (hal::I2cSwitcher::instance().wire().endTransmission() == 0) {
                devices.push_back(addr);
            }
        }

        return devices;
    }

    // Check if device is present at address
    bool isDevicePresent(uint8_t addr) {
        hal::I2cSwitcher::instance().use(sda_, scl_);
        hal::I2cSwitcher::instance().wire().beginTransmission(addr);
        return (hal::I2cSwitcher::instance().wire().endTransmission() == 0);
    }

    // Select this bus and return the shared TwoWire
    TwoWire& select() {
        hal::I2cSwitcher::instance().use(sda_, scl_);
        return hal::I2cSwitcher::instance().wire();
    }

    uint8_t getBusId() const { return busId_; }

    void printScanResults() {
        Serial.printf("I2C Bus %d (SDA=%d, SCL=%d) scan:\n", busId_, sda_, scl_);
        auto devices = scan();

        if (devices.empty()) {
            Serial.println("  No devices found");
        } else {
            for (uint8_t addr : devices) {
                Serial.printf("  Device at 0x%02X\n", addr);
            }
        }
    }

private:
    int sda_;
    int scl_;
    uint8_t busId_;
    // No owned TwoWire; uses shared Wire via I2cSwitcher
};

} // namespace hal
