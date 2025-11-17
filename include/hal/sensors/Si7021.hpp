#pragma once
#include "ISensor.hpp"
#include <Adafruit_Si7021.h>
#include <Arduino.h>
#include "Config.hpp"
#include "hal/I2cSwitcher.hpp"
#include "util/Logger.hpp"

namespace hal {

class Si7021 : public ISensor<app::Si7021Reading> {
public:
    // Si7021 has fixed I2C address 0x40 - no address customization
    explicit Si7021(TwoWire& wire, uint8_t busId = 0)
        : wire_(wire), busId_(busId) {}

    bool begin() override {
        hal::I2cSwitcher::instance().useBusId(busId_);
        LOG_DEBUG("[Si7021][bus %u][0x%02X] begin()", busId_, SI7021_ADDR);
        // Probe device at fixed address
        wire_.beginTransmission(SI7021_ADDR);
        if (wire_.endTransmission() != 0) {
            LOG_ERROR("[Si7021][bus %u][0x%02X] probe FAILED", busId_, SI7021_ADDR);
            return false;
        }

        if (!sensor_.begin()) {
            LOG_ERROR("[Si7021][bus %u][0x%02X] begin() FAILED", busId_, SI7021_ADDR);
            return false;
        }

        // Fetch serial number via device commands
        readSerialNumber_();

        return true;
    }

    util::Result<app::Si7021Reading, app::SensorError> read() override {
        hal::I2cSwitcher::instance().useBusId(busId_);
        LOG_DEBUG("[Si7021][bus %u][0x%02X] read() start", busId_, SI7021_ADDR);
        float temp = sensor_.readTemperature();
        float humidity = sensor_.readHumidity();

        // Check for invalid readings (sensor typically returns NAN on error)
        if (!std::isfinite(temp) || !std::isfinite(humidity)) {
            LOG_ERROR("[Si7021][bus %u][0x%02X] read() INVALID", busId_, SI7021_ADDR);
            return util::Result<app::Si7021Reading, app::SensorError>::Err(
                app::SensorError::ReadFailed);
        }

        app::Si7021Reading reading;
        reading.tempC = temp;
        reading.humidity = humidity;
        reading.serialNumber = serialNumber_;
        reading.valid = true;

        LOG_DEBUG("[Si7021][bus %u][0x%02X] T=%.2fC RH=%.2f%%",
                  busId_, SI7021_ADDR, reading.tempC, reading.humidity);
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
    void readSerialNumber_() {
        hal::I2cSwitcher::instance().useBusId(busId_);

        uint8_t sna[8] = {0};
        uint8_t snb[6] = {0};

        // First block
        wire_.beginTransmission(SI7021_ADDR);
        wire_.write(0xFA);
        wire_.write(0x0F);
        if (wire_.endTransmission() != 0) {
            LOG_ERROR("[Si7021][bus %u] SNA tx failed", busId_);
            return;
        }
        if (wire_.requestFrom((uint8_t)SI7021_ADDR, (uint8_t)8) != 8) {
            LOG_ERROR("[Si7021][bus %u] SNA rx failed", busId_);
            return;
        }
        for (int i = 0; i < 8; i++) sna[i] = wire_.read();

        // Second block
        wire_.beginTransmission(SI7021_ADDR);
        wire_.write(0xFC);
        wire_.write(0xC9);
        if (wire_.endTransmission() != 0) {
            LOG_ERROR("[Si7021][bus %u] SNB tx failed", busId_);
            return;
        }
        if (wire_.requestFrom((uint8_t)SI7021_ADDR, (uint8_t)6) != 6) {
            LOG_ERROR("[Si7021][bus %u] SNB rx failed", busId_);
            return;
        }
        for (int i = 0; i < 6; i++) snb[i] = wire_.read();

        // Extract serial bytes, skipping CRC bytes
        uint8_t sbytes[8];
        sbytes[0] = sna[0];
        sbytes[1] = sna[2];
        sbytes[2] = sna[4];
        sbytes[3] = sna[6];
        sbytes[4] = snb[0];
        sbytes[5] = snb[1];
        sbytes[6] = snb[3];
        sbytes[7] = snb[4];

        uint64_t serial = 0;
        for (int i = 0; i < 8; i++) {
            serial = (serial << 8) | sbytes[i];
        }
        serialNumber_ = serial;
        LOG_DEBUG("[Si7021][bus %u] Serial: %08llX%08llX", busId_,
                  (unsigned long long)(serial >> 32),
                  (unsigned long long)(serial & 0xFFFFFFFFULL));
    }

    static constexpr uint8_t SI7021_ADDR = 0x40;  // Fixed I2C address

    TwoWire& wire_;
    uint8_t busId_;
    uint64_t serialNumber_ = 0;
    Adafruit_Si7021 sensor_;
};

} // namespace hal
