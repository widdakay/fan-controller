#pragma once

#include "hal/sensors/SensorDescriptor.hpp"
#include "hal/sensors/Ads1115.hpp"
#include "util/Thermistor.hpp"
#include "app/Types.hpp"
#include <Arduino.h>

namespace hal {

/**
 * @brief Virtual sensor for thermistor connected to ADS1115 ADC
 *
 * Reads voltage from ADC, applies thermistor calculation, and returns temperature.
 * This allows thermistors to appear as regular sensors in the unified sensor list.
 */
class ThermistorSensor : public ISensorInstance {
public:
    /**
     * @brief Create thermistor sensor
     *
     * @param adc Pointer to ADS1115 instance
     * @param channel ADC channel (0-3)
     * @param thermistor Thermistor calculator
     * @param name Sensor name (e.g., "motor_ntc", "mcu_ntc")
     * @param busId I2C bus ID of the ADC
     * @param adcAddress I2C address of the ADC
     */
    ThermistorSensor(
        Ads1115* adc,
        uint8_t channel,
        const util::ThermistorSH& thermistor,
        const char* name,
        uint8_t busId,
        uint8_t adcAddress
    )
        : adc_(adc)
        , channel_(channel)
        , thermistor_(thermistor)
        , name_(name)
        , busId_(busId)
        , adcAddress_(adcAddress)
    {}

    const char* getTypeName() const override { return "Thermistor"; }
    const char* getMeasurementName() const override { return "thermistor"; }
    uint8_t getBusId() const override { return busId_; }
    uint8_t getAddress() const override { return adcAddress_; }
    bool isConnected() const override { return adc_ && adc_->isConnected(); }

    util::Result<String, app::SensorError> readAsJson() override {
        if (!adc_) {
            return util::Result<String, app::SensorError>::Err(app::SensorError::NotInitialized);
        }

        // Read ADC voltage
        auto voltResult = adc_->readVolts(channel_);
        if (!voltResult.isOk()) {
            return util::Result<String, app::SensorError>::Err(app::SensorError::ReadFailed);
        }

        float voltage = voltResult.value();

        // Read reference voltage (3.3V rail on channel 2)
        auto vrefResult = adc_->readVolts(2);
        float vref = vrefResult.isOk() ? vrefResult.value() * 2.0f : 3.3f; // Compensate for 2:1 divider

        // Calculate resistance and temperature
        float resistance = thermistor_.resistanceFromV(voltage, vref);
        float tempC = thermistor_.tempC_from_R(resistance);
        bool inRange = thermistor_.isValidRange(tempC);

        // Format as JSON
        String json = "{\"name\":\"" + String(name_) + "\"";
        json += ",\"temp_c\":" + String(tempC, 2);
        json += ",\"resistance\":" + String(resistance, 0);
        json += ",\"voltage\":" + String(voltage, 3);
        json += ",\"in_range\":" + String(inRange ? "true" : "false");
        json += "}";

        return util::Result<String, app::SensorError>::Ok(json);
    }

private:
    Ads1115* adc_;
    uint8_t channel_;
    util::ThermistorSH thermistor_;
    const char* name_;
    uint8_t busId_;
    uint8_t adcAddress_;
};

/**
 * @brief Virtual sensor for voltage rail monitoring from ADS1115
 *
 * Reads voltage from ADC and reports as power rail reading.
 */
class VoltageRailSensor : public ISensorInstance {
public:
    /**
     * @brief Create voltage rail sensor
     *
     * @param adc Pointer to ADS1115 instance
     * @param channel ADC channel (0-3)
     * @param dividerRatio Voltage divider ratio (e.g., 2.0 for 2:1 divider)
     * @param name Rail name (e.g., "3v3_rail", "5v_rail")
     * @param busId I2C bus ID of the ADC
     * @param adcAddress I2C address of the ADC
     */
    VoltageRailSensor(
        Ads1115* adc,
        uint8_t channel,
        float dividerRatio,
        const char* name,
        uint8_t busId,
        uint8_t adcAddress
    )
        : adc_(adc)
        , channel_(channel)
        , dividerRatio_(dividerRatio)
        , name_(name)
        , busId_(busId)
        , adcAddress_(adcAddress)
    {}

    const char* getTypeName() const override { return "VoltageRail"; }
    const char* getMeasurementName() const override { return "voltage_rail"; }
    uint8_t getBusId() const override { return busId_; }
    uint8_t getAddress() const override { return adcAddress_; }
    bool isConnected() const override { return adc_ && adc_->isConnected(); }

    util::Result<String, app::SensorError> readAsJson() override {
        if (!adc_) {
            return util::Result<String, app::SensorError>::Err(app::SensorError::NotInitialized);
        }

        // Read ADC voltage and apply divider compensation
        auto voltResult = adc_->readVolts(channel_);
        if (!voltResult.isOk()) {
            return util::Result<String, app::SensorError>::Err(app::SensorError::ReadFailed);
        }

        float voltage = voltResult.value() * dividerRatio_;

        // Format as JSON
        String json = "{\"name\":\"" + String(name_) + "\"";
        json += ",\"voltage\":" + String(voltage, 3);
        json += "}";

        return util::Result<String, app::SensorError>::Ok(json);
    }

private:
    Ads1115* adc_;
    uint8_t channel_;
    float dividerRatio_;
    const char* name_;
    uint8_t busId_;
    uint8_t adcAddress_;
};

} // namespace hal
