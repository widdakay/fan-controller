#pragma once

#include "hal/sensors/SensorDescriptor.hpp"
#include <vector>
#include <Arduino.h>

namespace hal {

/**
 * @brief Central registry for all sensor types
 *
 * Sensors register themselves statically. The Application scans all buses
 * and queries the registry to find matching sensor types.
 *
 * Usage:
 *   // In sensor header:
 *   static SensorDescriptor getDescriptor();
 *
 *   // In registry initialization:
 *   SensorRegistry::instance().registerSensor(MySensor::getDescriptor());
 *
 *   // In Application:
 *   auto descriptors = SensorRegistry::instance().findByAddress(0x76);
 *   for (auto& desc : descriptors) {
 *       auto sensor = desc.factory(bus, address);
 *       ...
 *   }
 */
class SensorRegistry {
public:
    /**
     * @brief Get singleton instance
     */
    static SensorRegistry& instance() {
        static SensorRegistry registry;
        return registry;
    }

    /**
     * @brief Register a sensor type
     */
    void registerSensor(const SensorDescriptor& descriptor) {
        descriptors_.push_back(descriptor);
        Serial.print("[Registry] Registered sensor type: ");
        Serial.println(descriptor.typeName);
    }

    /**
     * @brief Find all sensor descriptors matching the given I2C address
     */
    std::vector<const SensorDescriptor*> findByAddress(uint8_t address) const {
        std::vector<const SensorDescriptor*> matches;
        for (const auto& desc : descriptors_) {
            if (desc.matchesAddress(address)) {
                matches.push_back(&desc);
            }
        }
        return matches;
    }

    /**
     * @brief Get all registered sensor descriptors
     */
    const std::vector<SensorDescriptor>& getAllDescriptors() const {
        return descriptors_;
    }

    /**
     * @brief Get count of registered sensor types
     */
    size_t getCount() const {
        return descriptors_.size();
    }

    /**
     * @brief Print all registered sensors (for debugging)
     */
    void printRegistry() const {
        Serial.println("[Registry] Registered sensor types:");
        for (const auto& desc : descriptors_) {
            Serial.print("  - ");
            Serial.print(desc.typeName);
            Serial.print(" (");
            Serial.print(desc.measurementName);
            Serial.print(") @ addresses: ");
            for (size_t i = 0; i < desc.i2cAddresses.size(); i++) {
                if (i > 0) Serial.print(", ");
                Serial.print("0x");
                Serial.print(desc.i2cAddresses[i], HEX);
            }
            if (desc.supportsPostProcessing) {
                Serial.print(" [post-processing]");
            }
            Serial.println();
        }
    }

private:
    SensorRegistry() = default;
    std::vector<SensorDescriptor> descriptors_;
};

/**
 * @brief Helper class for static sensor registration
 *
 * Usage in sensor implementation:
 *   static SensorRegistrar<MySensor> registrar;
 */
template<typename TSensor>
class SensorRegistrar {
public:
    SensorRegistrar() {
        SensorRegistry::instance().registerSensor(TSensor::getDescriptor());
    }
};

} // namespace hal
