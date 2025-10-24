#pragma once
#include "util/Result.hpp"
#include "app/Types.hpp"
#include <cstdint>
#include <optional>

namespace hal {

// Base sensor interface
template<typename TReading>
class ISensor {
public:
    virtual ~ISensor() = default;

    // Initialize the sensor
    virtual bool begin() = 0;

    // Read sensor data
    virtual util::Result<TReading, app::SensorError> read() = 0;

    // Get sensor serial number if available
    virtual std::optional<uint64_t> getSerial() const {
        return std::nullopt;
    }

    // Get sensor name/type
    virtual const char* getName() const = 0;

    // Check if sensor is responsive
    virtual bool isConnected() const = 0;
};

} // namespace hal
