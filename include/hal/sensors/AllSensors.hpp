#pragma once

/**
 * @file AllSensors.hpp
 * @brief Master include for all sensor types
 *
 * This file includes all sensor implementations and triggers their static
 * registration with the SensorRegistry. Simply #include this file to make
 * all sensors available for automatic discovery.
 *
 * To add a new sensor:
 * 1. Create MyNewSensor.hpp implementing ISensor<MyReading>
 * 2. Create MyNewSensorWrapper.hpp with getMyNewSensorDescriptor()
 * 3. Add static registration in the wrapper
 * 4. #include the wrapper here
 */

// Core sensor infrastructure
#include "hal/sensors/ISensor.hpp"
#include "hal/sensors/SensorDescriptor.hpp"
#include "hal/sensors/SensorRegistry.hpp"
#include "hal/sensors/VirtualSensor.hpp"
#include "util/Logger.hpp"

// ADC and power monitoring (with post-processing)
#include "hal/sensors/Ads1115Sensor.hpp"
#include "hal/sensors/Ina226Sensor.hpp"

// Environmental sensors
#include "hal/sensors/EnvironmentalSensors.hpp"

namespace hal {

/**
 * @brief Initialize all sensor registrations
 *
 * Call this once during Application setup to ensure all static registrations
 * have completed. This is a no-op but ensures the static objects are created.
 */
inline void initializeSensorRegistry() {
    // The act of calling this function ensures all static registrars have run
    LOG_INFO("[AllSensors] Registry initialized with %d sensor types",
             SensorRegistry::instance().getCount());
}

} // namespace hal
