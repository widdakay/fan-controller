#pragma once

#include "hal/sensors/SensorDescriptor.hpp"
#include "hal/sensors/SensorRegistry.hpp"
#include "hal/sensors/VirtualSensor.hpp"
#include "hal/sensors/Ads1115.hpp"
#include "util/Thermistor.hpp"
#include "app/Types.hpp"
#include <Arduino.h>

namespace hal {

/**
 * @brief Sensor wrapper for ADS1115 ADC
 *
 * The ADS1115 is special - it's a multi-channel ADC that doesn't directly
 * measure a physical quantity. Instead, it creates "virtual sensors" for
 * thermistors and voltage rails connected to its channels.
 */
class Ads1115SensorInstance : public ISensorInstance {
public:
    Ads1115SensorInstance(std::unique_ptr<Ads1115> adc, uint8_t busId, uint8_t address)
        : adc_(std::move(adc))
        , busId_(busId)
        , address_(address)
    {}

    const char* getTypeName() const override { return "ADS1115"; }
    const char* getMeasurementName() const override { return "adc"; }
    uint8_t getBusId() const override { return busId_; }
    uint8_t getAddress() const override { return address_; }
    bool isConnected() const override { return adc_ && adc_->isConnected(); }

    bool needsPostProcessing() const override { return true; }

    util::Result<String, app::SensorError> readAsJson() override {
        // The ADC itself doesn't report - its virtual sensors do
        // But we can report raw channel voltages if needed
        if (!adc_) {
            return util::Result<String, app::SensorError>::Err(app::SensorError::NotInitialized);
        }

        String json = "{";
        for (uint8_t ch = 0; ch < 4; ch++) {
            auto result = adc_->readVolts(ch);
            if (result.isOk()) {
                if (ch > 0) json += ",";
                json += "\"ch" + String(ch) + "_v\":" + String(result.value(), 4);
            }
        }
        json += "}";

        return util::Result<String, app::SensorError>::Ok(json);
    }

    std::vector<std::unique_ptr<ISensorInstance>> createPostProcessedSensors() override {
        std::vector<std::unique_ptr<ISensorInstance>> sensors;

        if (!adc_) return sensors;

        // Create thermistor calculator (10k NTC, Murata coefficients)
        util::ThermistorSH thermistor(10000.0f, 8.688e-4f, 2.547e-4f, 1.781e-7f);

        // Channel 0: Motor NTC thermistor
        sensors.push_back(std::make_unique<ThermistorSensor>(
            adc_.get(), 0, thermistor, "motor_ntc", busId_, address_
        ));

        // Channel 1: MCU/Board NTC thermistor
        sensors.push_back(std::make_unique<ThermistorSensor>(
            adc_.get(), 1, thermistor, "mcu_ntc", busId_, address_
        ));

        // Channel 2: 3.3V rail (2:1 voltage divider)
        sensors.push_back(std::make_unique<VoltageRailSensor>(
            adc_.get(), 2, 2.0f, "3v3_rail", busId_, address_
        ));

        // Channel 3: 5V rail (2:1 voltage divider)
        sensors.push_back(std::make_unique<VoltageRailSensor>(
            adc_.get(), 3, 2.0f, "5v_rail", busId_, address_
        ));

        Serial.printf("[ADS1115][bus %u][0x%02X] Created %d post-processed sensors\n",
                      busId_, address_, sensors.size());

        return sensors;
    }

    /**
     * @brief Get sensor descriptor for registration
     */
    static SensorDescriptor getDescriptor() {
        SensorDescriptor desc;
        desc.typeName = "ADS1115";
        desc.measurementName = "adc";
        desc.i2cAddresses = {0x48, 0x49, 0x4A, 0x4B}; // Address depends on ADDR pin
        desc.supportsPostProcessing = true;

        desc.factory = [](I2cBus& bus, uint8_t addr) -> std::unique_ptr<ISensorInstance> {
            auto adc = std::make_unique<Ads1115>(bus.select(), addr, bus.getBusId());
            if (!adc->begin()) {
                return nullptr;
            }
            return std::make_unique<Ads1115SensorInstance>(std::move(adc), bus.getBusId(), addr);
        };

        return desc;
    }

private:
    std::unique_ptr<Ads1115> adc_;
    uint8_t busId_;
    uint8_t address_;
};

// Static registration
static SensorRegistrar<Ads1115SensorInstance> ads1115Registrar;

} // namespace hal
