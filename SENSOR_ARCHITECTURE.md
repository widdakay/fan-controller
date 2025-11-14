# Unified Sensor Architecture

## Overview

The firmware now uses a unified sensor discovery and management system that treats all I2C buses equally. Sensors are automatically discovered through a registry pattern, and new sensors can be added by simply creating a new file and implementing required functions.

## Architecture Components

### 1. Core Infrastructure (`include/hal/sensors/`)

#### `ISensorInstance` Interface
- Type-erased base class for all sensors
- Provides common interface: `readAsJson()`, `getBusId()`, `getAddress()`, `getTypeName()`, etc.
- Supports optional post-processing (e.g., ADC creates thermistor virtual sensors)

#### `SensorDescriptor` Struct
Each sensor type provides a descriptor containing:
- Type name (e.g., "BME688", "ADS1115")
- Telemetry measurement name (e.g., "env", "power")
- I2C addresses to scan for
- Factory function to create sensor instances
- Post-processing flag

#### `SensorRegistry` Singleton
- Central repository of all sensor types
- Sensors auto-register on startup via static initialization
- Provides `findByAddress()` to match I2C addresses to sensor types

#### `ConcreteSensorInstance<TSensor, TReading>` Template
- Wraps existing `ISensor<TReading>` implementations
- Handles JSON serialization per sensor type
- Manages sensor lifecycle

### 2. Virtual Sensors (`include/hal/sensors/VirtualSensor.hpp`)

Post-processed sensors for ADC-based measurements:

#### `ThermistorSensor`
- Reads voltage from ADS1115 channel
- Applies Steinhart-Hart thermistor calculation
- Reports temperature, resistance, voltage
- Examples: motor_ntc, mcu_ntc

#### `VoltageRailSensor`
- Reads voltage from ADS1115 channel
- Applies voltage divider compensation
- Examples: 3v3_rail, 5v_rail

### 3. Sensor Implementations

All sensors now register themselves statically:

- **ADS1115** (`Ads1115Sensor.hpp`): Multi-channel ADC with post-processing
  - Creates 4 virtual sensors: 2 thermistors + 2 voltage rails
  - Addresses: 0x48, 0x49, 0x4A, 0x4B

- **INA226** (`Ina226Sensor.hpp`): Power monitor
  - Measures bus voltage, current, power
  - Addresses: 0x40, 0x41, 0x44, 0x45

- **BME688** (`EnvironmentalSensors.hpp`): Environmental sensor
  - Temperature, humidity, pressure, gas resistance
  - Addresses: 0x76, 0x77

- **Si7021** (`EnvironmentalSensors.hpp`): Temp/humidity sensor
  - Temperature, humidity, serial number
  - Address: 0x40 (fixed)

- **AHT20** (`EnvironmentalSensors.hpp`): Temp/humidity sensor
  - Temperature, humidity
  - Address: 0x38 (fixed)

- **ZMOD4510** (`EnvironmentalSensors.hpp`): Air quality sensor (placeholder)
  - AQI, ozone, NO2
  - Address: 0x32 (fixed)

### 4. Application Integration

#### Unified Discovery (`Application::discoverAllSensors_()`)
```cpp
// Scans ALL I2C buses (0-4) equally
for (uint8_t busId = 0; busId <= 4; busId++) {
    // Create bus, scan for devices
    for (uint8_t addr : devices) {
        // Query registry for matching sensors
        auto descriptors = SensorRegistry::instance().findByAddress(addr);
        // Try each descriptor's factory until one succeeds
        // Handle post-processing (ADC creates virtual sensors)
    }
}
```

#### Unified Reading (`Application::readAndReportSensors_()`)
```cpp
// Single loop through all sensors
for (const auto& sensor : sensors_) {
    auto jsonResult = sensor->readAsJson();
    // Parse JSON and send to telemetry
    telemetry_->sendSensorData(
        sensor->getMeasurementName(),
        sensor->getBusId(),
        fields,
        sensor->getSerial()
    );
}
```

## Adding a New Sensor

### Step 1: Implement ISensor<TReading>
```cpp
// include/hal/sensors/MyNewSensor.hpp
class MyNewSensor : public ISensor<MyReading> {
public:
    explicit MyNewSensor(TwoWire& wire, uint8_t addr, uint8_t busId);
    bool begin() override;
    Result<MyReading, SensorError> read() override;
    const char* getName() const override { return "MyNewSensor"; }
    bool isConnected() const override;
};
```

### Step 2: Add Reading Type (if new)
```cpp
// include/app/Types.hpp
struct MyReading {
    float value1;
    float value2;
    bool valid;
};
```

### Step 3: Create Descriptor and Register
```cpp
// include/hal/sensors/MyNewSensorWrapper.hpp
#include "hal/sensors/SensorDescriptor.hpp"
#include "hal/sensors/SensorRegistry.hpp"
#include "hal/sensors/MyNewSensor.hpp"

namespace hal {

// JSON serialization specialization
template<> inline String
ConcreteSensorInstance<MyNewSensor, app::MyReading>::formatReadingAsJson(
    const app::MyReading& r
) {
    if (!r.valid) return "{}";
    return "{\"value1\":" + String(r.value1, 2) +
           ",\"value2\":" + String(r.value2, 2) + "}";
}

// Descriptor
inline SensorDescriptor getMyNewSensorDescriptor() {
    SensorDescriptor desc;
    desc.typeName = "MyNewSensor";
    desc.measurementName = "my_measurement";
    desc.i2cAddresses = {0xAB}; // Your sensor's I2C address(es)
    desc.supportsPostProcessing = false;

    desc.factory = [](I2cBus& bus, uint8_t addr) -> std::unique_ptr<ISensorInstance> {
        auto sensor = std::make_unique<MyNewSensor>(
            bus.select(), addr, bus.getBusId()
        );
        if (!sensor->begin()) {
            return nullptr;
        }
        return std::make_unique<ConcreteSensorInstance<MyNewSensor, app::MyReading>>(
            std::move(sensor), "MyNewSensor", "my_measurement",
            bus.getBusId(), addr
        );
    };

    return desc;
}

// Static registration
struct MyNewSensorRegistrar {
    MyNewSensorRegistrar() {
        SensorRegistry::instance().registerSensor(getMyNewSensorDescriptor());
    }
};

static MyNewSensorRegistrar myNewSensorRegistrar;

} // namespace hal
```

### Step 4: Include in AllSensors.hpp
```cpp
// include/hal/sensors/AllSensors.hpp
#include "hal/sensors/MyNewSensorWrapper.hpp"
```

### Done!
The sensor will now be automatically discovered on any I2C bus and reported to telemetry.

## Key Benefits

### 1. **Unified I2C Bus Treatment**
- All buses (0-4) scanned equally
- No hardcoded "onboard" vs "external" distinction
- Sensors can appear on any bus

### 2. **Automatic Discovery**
- Registry pattern eliminates manual initialization code
- Sensors register themselves via static initialization
- Application code is sensor-agnostic

### 3. **Easy Extensibility**
- Add new sensor = create 1 file + 1 include line
- All boilerplate (scanning, initialization, telemetry) handled automatically
- Type-safe via templates

### 4. **Post-Processing Support**
- Sensors can create "virtual sensors" (e.g., ADC → thermistors)
- Post-processing only applied when base sensor is found
- Clean separation of concerns

### 5. **Centralized Sensor Information**
- All sensor metadata in descriptors
- Single source of truth for addresses, names, factories
- Easy to audit and maintain

## Memory Usage

**Current build results:**
- RAM: 15.4% (50,300 bytes / 327,680 bytes)
- Flash: 77.3% (1,013,593 bytes / 1,310,720 bytes)

Slightly increased from previous 76.2% flash due to registry system, but well within limits.

## Example Boot Sequence

```
=== ESP32 Air Quality Controller ===
Firmware: 1.0.0
Chip ID: 123456789abc

Initializing hardware...
[Registry] Registered sensor type: ADS1115
[Registry] Registered sensor type: INA226
[Registry] Registered sensor type: BME688
[Registry] Registered sensor type: Si7021
[Registry] Registered sensor type: AHT20
[Registry] Registered sensor type: ZMOD4510
[Registry] Registry initialized with 6 sensor types

Discovering sensors on all I2C buses...

=== I2C Bus 0 (SDA=1, SCL=2) ===
  Found 2 device(s):
    0x40: trying INA226... OK
    0x48: trying ADS1115... OK
[ADS1115][bus 0][0x48] Created 4 post-processed sensors

=== I2C Bus 1 (SDA=11, SCL=12) ===
  Found 2 device(s):
    0x38: trying AHT20... OK
    0x76: trying BME688... OK

=== Sensor Discovery Complete ===
Total sensors discovered: 7
Sensor types:
  ADS1115: 1
  INA226: 1
  Thermistor: 2
  VoltageRail: 2
  BME688: 1
  AHT20: 1
```

## Files Created/Modified

### New Files
- `include/hal/sensors/SensorDescriptor.hpp` - Core sensor abstraction
- `include/hal/sensors/SensorRegistry.hpp` - Central sensor registry
- `include/hal/sensors/VirtualSensor.hpp` - Post-processed sensors
- `include/hal/sensors/Ads1115Sensor.hpp` - ADS1115 wrapper with post-processing
- `include/hal/sensors/Ina226Sensor.hpp` - INA226 wrapper
- `include/hal/sensors/EnvironmentalSensors.hpp` - BME688, Si7021, AHT20, ZMOD4510
- `include/hal/sensors/AllSensors.hpp` - Master include for all sensors
- `SENSOR_ARCHITECTURE.md` - This documentation

### Modified/Moved Files
- `include/app/Application.hpp` - Simplified to unified sensor storage
- `src/app/Application.cpp` - New discovery and reading methods
- `include/hal/sensors/Ina226.hpp` - Now implements ISensor interface (moved from hal/)
- `include/hal/sensors/Ads1115.hpp` - Added isConnected() method (moved from hal/)
- `include/hal/sensors/IAdc.hpp` - Added isConnected() to interface (moved from hal/)

### Folder Structure
**`include/hal/`** - Infrastructure and actuators:
- `I2cBus.hpp`, `I2cSwitcher.hpp` - I2C bus management
- `LedController.hpp` - LED control
- `MotorController.hpp` - Motor control
- `OneWireBus.hpp` - OneWire bus management

**`include/hal/sensors/`** - All sensors and sensor infrastructure:
- `ISensor.hpp`, `IAdc.hpp` - Sensor interfaces
- `SensorDescriptor.hpp`, `SensorRegistry.hpp` - Registration system
- `Ads1115.hpp`, `Ina226.hpp`, `Bme688.hpp`, etc. - Sensor implementations
- `Ads1115Sensor.hpp`, `Ina226Sensor.hpp`, etc. - Sensor wrappers
- `VirtualSensor.hpp` - Post-processed sensors
- `AllSensors.hpp` - Master include

## Future Enhancements

1. **Sensor Filtering**: Allow configuration to disable certain sensor types
2. **Hot-Plugging**: Periodic re-scanning for dynamically added sensors
3. **Sensor Health**: Track read failure rates, automatic sensor removal
4. **Custom Post-Processors**: Plugin system for user-defined transformations
5. **Multi-Address Sensors**: Support sensors with multiple I2C addresses (e.g., sensor + EEPROM)
