#pragma once

#include "hal/sensors/SensorDescriptor.hpp"
#include "hal/sensors/SensorRegistry.hpp"
#include "hal/sensors/Bme688.hpp"
#include "hal/sensors/Si7021.hpp"
#include "hal/sensors/Aht20.hpp"
#include "hal/sensors/Zmod4510.hpp"
#include "app/Types.hpp"
#include "Config.hpp"
#include <Arduino.h>

namespace hal {

// ============================================================================
// BME688 Environmental Sensor
// ============================================================================

inline SensorDescriptor getBme688Descriptor() {
    SensorDescriptor desc;
    desc.typeName = "BME688";
    desc.measurementName = "bme688";
    desc.i2cAddresses = {0x76, 0x77}; // Address depends on SDO pin
    desc.supportsPostProcessing = false;

    desc.factory = [](I2cBus& bus, uint8_t addr) -> std::unique_ptr<ISensorInstance> {
        auto sensor = std::make_unique<Bme688>(bus.select(), addr, bus.getBusId());
        if (!sensor->begin()) {
            return nullptr;
        }
        return std::make_unique<ConcreteSensorInstance<Bme688, app::Bme688Reading>>(
            std::move(sensor), "BME688", "bme688", bus.getBusId(), addr
        );
    };

    return desc;
}

// ============================================================================
// Si7021 Temperature & Humidity Sensor
// ============================================================================

inline SensorDescriptor getSi7021Descriptor() {
    SensorDescriptor desc;
    desc.typeName = "Si7021";
    desc.measurementName = "si7021";
    desc.i2cAddresses = {0x40}; // Fixed address
    desc.supportsPostProcessing = false;

    desc.factory = [](I2cBus& bus, uint8_t addr) -> std::unique_ptr<ISensorInstance> {
        auto sensor = std::make_unique<Si7021>(bus.select(), bus.getBusId());
        if (!sensor->begin()) {
            return nullptr;
        }
        return std::make_unique<ConcreteSensorInstance<Si7021, app::Si7021Reading>>(
            std::move(sensor), "Si7021", "si7021", bus.getBusId(), addr
        );
    };

    return desc;
}

// ============================================================================
// AHT20 Temperature & Humidity Sensor
// ============================================================================

inline SensorDescriptor getAht20Descriptor() {
    SensorDescriptor desc;
    desc.typeName = "AHT20";
    desc.measurementName = "aht20";
    desc.i2cAddresses = {config::I2C_ADDR_AHT20}; // Typically 0x38
    desc.supportsPostProcessing = false;

    desc.factory = [](I2cBus& bus, uint8_t addr) -> std::unique_ptr<ISensorInstance> {
        auto sensor = std::make_unique<Aht20>(bus.select(), bus.getBusId(), addr);
        if (!sensor->begin()) {
            return nullptr;
        }
        return std::make_unique<ConcreteSensorInstance<Aht20, app::Si7021Reading>>(
            std::move(sensor), "AHT20", "aht20", bus.getBusId(), addr
        );
    };

    return desc;
}

// ============================================================================
// ZMOD4510 Air Quality Sensor
// ============================================================================

inline SensorDescriptor getZmod4510Descriptor() {
    SensorDescriptor desc;
    desc.typeName = "ZMOD4510";
    desc.measurementName = "zmod4510";
    desc.i2cAddresses = {0x32}; // Fixed address
    desc.supportsPostProcessing = false;

    desc.factory = [](I2cBus& bus, uint8_t addr) -> std::unique_ptr<ISensorInstance> {
        auto sensor = std::make_unique<Zmod4510>(bus.select(), addr, bus.getBusId());
        if (!sensor->begin()) {
            return nullptr;
        }
        return std::make_unique<ConcreteSensorInstance<Zmod4510, app::Zmod4510Reading>>(
            std::move(sensor), "ZMOD4510", "zmod4510", bus.getBusId(), addr
        );
    };

    return desc;
}

// ============================================================================
// Static Registration
// ============================================================================

struct EnvironmentalSensorsRegistrar {
    EnvironmentalSensorsRegistrar() {
        SensorRegistry::instance().registerSensor(getBme688Descriptor());
        SensorRegistry::instance().registerSensor(getSi7021Descriptor());
        SensorRegistry::instance().registerSensor(getAht20Descriptor());
        SensorRegistry::instance().registerSensor(getZmod4510Descriptor());
    }
};

static EnvironmentalSensorsRegistrar environmentalSensorsRegistrar;

} // namespace hal
