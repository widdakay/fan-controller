#pragma once
#include <Wire.h>
#include <memory>
#include <vector>

namespace hal {

// I2C bus wrapper with scanning and recovery capabilities
class I2cBus {
public:
    explicit I2cBus(int sda, int scl, uint8_t busId)
        : sda_(sda), scl_(scl), busId_(busId), wire_(new TwoWire(busId)) {}

    bool begin(uint32_t frequency = 100000) {
        wire_->begin(sda_, scl_, frequency);
        return true;
    }

    // Scan for I2C devices on this bus
    std::vector<uint8_t> scan() {
        std::vector<uint8_t> devices;

        for (uint8_t addr = 1; addr < 127; addr++) {
            wire_->beginTransmission(addr);
            if (wire_->endTransmission() == 0) {
                devices.push_back(addr);
            }
        }

        return devices;
    }

    // Check if device is present at address
    bool isDevicePresent(uint8_t addr) {
        wire_->beginTransmission(addr);
        return (wire_->endTransmission() == 0);
    }

    // Get the underlying TwoWire object
    TwoWire& getWire() { return *wire_; }

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
    std::unique_ptr<TwoWire> wire_;
};

} // namespace hal
