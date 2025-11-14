#pragma once

#include "hal/I2cBus.hpp"
#include "app/Types.hpp"
#include "util/Result.hpp"
#include <functional>
#include <vector>
#include <memory>
#include <optional>
#include <Arduino.h>

namespace hal {

// Forward declaration
class ISensorInstance;

/**
 * @brief Descriptor for a sensor type with factory and metadata
 *
 * Each sensor type provides a descriptor that includes:
 * - Detection criteria (I2C addresses)
 * - Factory function to create instances
 * - Metadata (name, measurement type)
 * - Whether it supports post-processing
 */
struct SensorDescriptor {
    // Human-readable sensor type name (e.g., "BME688", "ADS1115")
    const char* typeName;

    // Telemetry measurement name (e.g., "env", "power", "adc")
    const char* measurementName;

    // List of I2C addresses this sensor might respond to
    std::vector<uint8_t> i2cAddresses;

    // Factory function: creates sensor instance on given bus at given address
    // Returns nullptr if initialization fails
    using FactoryFunc = std::function<std::unique_ptr<ISensorInstance>(I2cBus&, uint8_t)>;
    FactoryFunc factory;

    // Whether this sensor supports post-processing (like ADC -> thermistor)
    bool supportsPostProcessing = false;

    /**
     * @brief Check if this sensor type matches the given I2C address
     */
    bool matchesAddress(uint8_t addr) const {
        for (auto candidate : i2cAddresses) {
            if (candidate == addr) return true;
        }
        return false;
    }
};

/**
 * @brief Type-erased sensor instance wrapper
 *
 * Allows storing different sensor types in a homogeneous collection
 */
class ISensorInstance {
public:
    virtual ~ISensorInstance() = default;

    // Get sensor type name
    virtual const char* getTypeName() const = 0;

    // Get telemetry measurement name
    virtual const char* getMeasurementName() const = 0;

    // Get bus this sensor is on
    virtual uint8_t getBusId() const = 0;

    // Get I2C address
    virtual uint8_t getAddress() const = 0;

    // Get optional serial number
    virtual std::optional<uint64_t> getSerial() const { return std::nullopt; }

    // Read sensor and format as JSON fields string
    // Returns Result with JSON string like: {"temp_c": 25.3, "humidity": 45.2}
    virtual util::Result<String, app::SensorError> readAsJson() = 0;

    // Check if sensor is still connected
    virtual bool isConnected() const = 0;

    // Whether this sensor needs post-processing
    virtual bool needsPostProcessing() const { return false; }

    // Create post-processed sensors (e.g., thermistors from ADC)
    // Returns empty vector if no post-processing needed
    virtual std::vector<std::unique_ptr<ISensorInstance>> createPostProcessedSensors() {
        return {};
    }
};

/**
 * @brief Concrete sensor instance wrapper template
 *
 * Wraps any sensor implementing ISensor<TReading> interface
 */
template<typename TSensor, typename TReading>
class ConcreteSensorInstance : public ISensorInstance {
public:
    ConcreteSensorInstance(
        std::unique_ptr<TSensor> sensor,
        const char* typeName,
        const char* measurementName,
        uint8_t busId,
        uint8_t address
    )
        : sensor_(std::move(sensor))
        , typeName_(typeName)
        , measurementName_(measurementName)
        , busId_(busId)
        , address_(address)
    {}

    const char* getTypeName() const override { return typeName_; }
    const char* getMeasurementName() const override { return measurementName_; }
    uint8_t getBusId() const override { return busId_; }
    uint8_t getAddress() const override { return address_; }

    std::optional<uint64_t> getSerial() const override {
        return sensor_->getSerial();
    }

    bool isConnected() const override {
        return sensor_->isConnected();
    }

    util::Result<String, app::SensorError> readAsJson() override {
        auto result = sensor_->read();
        if (!result.isOk()) {
            return util::Result<String, app::SensorError>::Err(result.error());
        }

        TReading reading = result.value();
        String json = formatReadingAsJson(reading);
        return util::Result<String, app::SensorError>::Ok(json);
    }

    // Get underlying sensor (for post-processing)
    TSensor* getSensor() { return sensor_.get(); }

private:
    std::unique_ptr<TSensor> sensor_;
    const char* typeName_;
    const char* measurementName_;
    uint8_t busId_;
    uint8_t address_;

    // Format reading as JSON string
    String formatReadingAsJson(const TReading& reading);
};

// Template specializations for different reading types
template<> inline String ConcreteSensorInstance<class Bme688, app::Bme688Reading>::formatReadingAsJson(const app::Bme688Reading& r) {
    if (!r.valid) return "{}";
    return "{\"temp_c\":" + String(r.tempC, 2) +
           ",\"humidity\":" + String(r.humidity, 1) +
           ",\"pressure_pa\":" + String(r.pressurePa, 0) +
           ",\"gas_resistance\":" + String(r.gasResistance, 0) + "}";
}

template<> inline String ConcreteSensorInstance<class Si7021, app::Si7021Reading>::formatReadingAsJson(const app::Si7021Reading& r) {
    if (!r.valid) return "{}";
    return "{\"temp_c\":" + String(r.tempC, 2) +
           ",\"humidity\":" + String(r.humidity, 1) + "}";
}

template<> inline String ConcreteSensorInstance<class Aht20, app::Si7021Reading>::formatReadingAsJson(const app::Si7021Reading& r) {
    if (!r.valid) return "{}";
    return "{\"temp_c\":" + String(r.tempC, 2) +
           ",\"humidity\":" + String(r.humidity, 1) + "}";
}

template<> inline String ConcreteSensorInstance<class Zmod4510, app::Zmod4510Reading>::formatReadingAsJson(const app::Zmod4510Reading& r) {
    if (!r.valid) return "{}";
    return "{\"temp_c\":" + String(r.tempC, 2) +
           ",\"humidity\":" + String(r.humidity, 1) +
           ",\"aqi\":" + String(r.aqi, 0) +
           ",\"ozone_ppb\":" + String(r.ozonePpb, 1) +
           ",\"no2_ppb\":" + String(r.no2Ppb, 1) + "}";
}

template<> inline String ConcreteSensorInstance<class Ina226, app::PowerReading>::formatReadingAsJson(const app::PowerReading& r) {
    if (!r.valid) return "{}";
    // Use same field names as original telemetry for compatibility
    return "{\"v_in\":" + String(r.busVolts, 6) +
           ",\"i_in\":" + String(r.currentMilliamps / 1000.0f, 6) +
           ",\"v_shunt\":" + String(r.shuntMillivolts, 6) +
           ",\"p_in\":" + String(r.powerMilliwatts / 1000.0f, 6) +
           ",\"overflow\":" + String(r.overflow ? "true" : "false") + "}";
}

} // namespace hal
