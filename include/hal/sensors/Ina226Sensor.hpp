#pragma once

#include "hal/sensors/SensorDescriptor.hpp"
#include "hal/sensors/SensorRegistry.hpp"
#include "hal/sensors/Ina226.hpp"
#include "app/Types.hpp"
#include <Arduino.h>

namespace hal {

/**
 * @brief Get sensor descriptor for INA226 registration
 */
inline SensorDescriptor getIna226Descriptor() {
    SensorDescriptor desc;
    desc.typeName = "INA226";
    desc.measurementName = "ina226";
    desc.i2cAddresses = {0x40, 0x41, 0x44, 0x45}; // Address depends on A0/A1 pins
    desc.supportsPostProcessing = false;

    desc.factory = [](I2cBus& bus, uint8_t addr) -> std::unique_ptr<ISensorInstance> {
        auto ina = std::make_unique<Ina226>(bus.select(), addr, bus.getBusId());
        if (!ina->begin(0.001f)) { // 1 milliohm shunt resistor
            return nullptr;
        }
        return std::make_unique<ConcreteSensorInstance<Ina226, app::PowerReading>>(
            std::move(ina), "INA226", "ina226", bus.getBusId(), addr
        );
    };

    return desc;
}

// Static registration
struct Ina226Registrar {
    Ina226Registrar() {
        SensorRegistry::instance().registerSensor(getIna226Descriptor());
    }
};

static Ina226Registrar ina226Registrar;

} // namespace hal
