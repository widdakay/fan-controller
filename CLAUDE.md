# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-S3 firmware for an air quality monitoring and fan control system. This is a **header-only** C++17 embedded project using PlatformIO with the Arduino framework. All implementation is in `.hpp` files in the `include/` directory.

## Build Commands

```bash
# Build firmware
pio run

# Clean build
pio run --target clean

# Upload via USB (first time)
pio run --target upload

# Upload via WiFi OTA (after initial USB flash)
pio run --target upload --upload-port UnderHouseFan.local
# Or by IP: --upload-port 192.168.1.xxx

# Monitor serial output (USB only)
pio device monitor

# Check device is online
pio device list
```

## Critical Configuration

Before first deployment, **must** edit `include/Config.hpp`:
- WiFi credentials array at line ~85 (`WIFI_CREDENTIALS`)
- MQTT server address at line ~93 (`MQTT_SERVER`)
- API endpoints at lines ~100-101 (`API_INFLUXDB`, `API_FW_UPDATE`)

Firmware version is in `platformio.ini` line 16: `-DFIRMWARE_VERSION=\"1.0.0\"`

## Architecture: 3-Layer Design

### 1. Application Layer (`include/app/`)
**Entry point**: `Application::setup()` and `Application::loop()` in `Application.hpp`

The `Application` class is the composition root that:
- Owns all HAL components (as `std::unique_ptr`)
- Owns all services (as `std::unique_ptr`)
- Uses `TaskScheduler` for non-blocking periodic operations
- Initializes in sequence: hardware → WiFi → services → task registration

**Critical flow**:
- `setup()`: One-time initialization with specific ordering (watchdog → LEDs → boot report → hardware → WiFi → services → tasks)
- `loop()`: Continuous non-blocking execution (watchdog feed → LED updates → OTA handle → MQTT loop → task scheduler tick)

### 2. Services Layer (`include/services/`)
Network and system services with specific responsibilities:

- **WiFiManager**: Multi-SSID scanning with signal strength selection. Returns `Result<void, WiFiError>`
- **MqttClient**: Callback-based message handling. User sets callback via `setMessageCallback()` for motor control commands
- **TelemetryService**: JSON formatting for InfluxDB. Always includes chip ID and device name in tags, `arduino_millis` in fields
- **OtaManager**: Dual OTA (ArduinoOTA + HTTPS). Callback for LED status updates during OTA
- **WatchdogService**: ESP32 task watchdog. **Must** call `feed()` regularly in main loop

### 3. HAL Layer (`include/hal/`)
Hardware abstraction with interface-based design:

- **Interfaces**: `IAdc`, `ISensor<TReading>` for testability
- **Sensor Pattern**: All sensors return `Result<TReading, SensorError>` from `read()` method
- **I2C Buses**: Multiple `I2cBus` instances (onboard + 3 external). Each wraps a `TwoWire` object
- **OneWire**: Temperature conversion is **non-blocking**. Must call `requestTemperatures()`, wait 800ms (timer), then `readAll()`
- **MotorController**: Safety features - deadtime on direction change (2ms), EN pin readback for fault detection

## Error Handling Pattern

Uses `Result<T, E>` from `util/Result.hpp` instead of exceptions:

```cpp
// Check result
auto result = sensor.read();
if (result.isOk()) {
    float value = result.value();
} else {
    SensorError err = result.error();
}

// Chain operations
float temp = adc->readVolts(0)
    .map([](float v) { return thermistor.resistanceFromV(v, 3.3f); })
    .valueOr(NAN);
```

Error enums in `app/Types.hpp`: `I2cError`, `SensorError`, `WiFiError`, `HttpError`, `MqttError`

## Memory Management Rules

1. **No dynamic allocation in loops** - Use `std::array` or pre-allocated `RingBuffer`
2. **Ownership via `std::unique_ptr`** - All HAL/service components owned by `Application`
3. **Vectors only for discovery** - Sensor detection at boot populates vectors, then read-only
4. **Stack allocation preferred** - Temporary readings use stack variables

## Task Scheduler Pattern

Non-blocking periodic tasks using `app/Tasks.hpp`:

```cpp
// Registration in Application::registerTasks_()
scheduler_.addTask("task_name", [this]() {
    // Task implementation - must be non-blocking
}, INTERVAL_MS);

// Execution in loop()
scheduler_.tick();  // Runs due tasks automatically
```

**Critical**: Task functions must not block. For multi-step operations (like OneWire), use state flags and timers.

## Sensor Integration

To add a new I2C sensor:

1. Create `include/hal/sensors/NewSensor.hpp` implementing `ISensor<NewReading>`
2. Add reading struct to `app/Types.hpp`
3. In `Application::initializeExternalSensors_()`: Scan buses, instantiate sensor, push to vector
4. In `Application::readAndReportSensors_()`: Iterate vector, call `read()`, send to telemetry

**Library API compatibility**: Check library enum names (e.g., INA226 uses `INA226_AVERAGE_16` not `AVERAGE_16`)

## MQTT Motor Control Flow

1. External system publishes float 0.0-1.0 to `home/fan1/power`
2. `MqttClient::messageCallback()` receives, parses, calls user callback
3. `Application::handleMqttMessage_()` calls `motor_->setFromMqtt(value)`
4. Every 10s, `publishMqttStatus_()` task publishes current power to `home/fan1/power/status`

## LED Status Indicators

Managed by `hal/LedController.hpp`:
- **Green**: Heartbeat every 1s (task)
- **Red**: Error flash 500ms (on HTTPS failure)
- **Orange**: OTA update in progress (callback from OtaManager)
- **Blue**: Motor running (duty > 1%)

## Telemetry Data Format

InfluxDB JSON via `TelemetryService`:

```json
{
  "measurement": "ESP_Health",
  "tags": {
    "device": "UnderHouseFan",
    "chip_id": "ABCD1234"
  },
  "fields": {
    "arduino_millis": 123456,
    "motor_temp_c": 45.2,
    ...
  }
}
```

**Always** include: device name, chip ID in tags; `arduino_millis` in fields.

## C++17 Features Used

- `std::unique_ptr`, `std::optional`, `std::vector`
- `inline constexpr` for zero-cost config constants
- Template interfaces (`ISensor<T>`)
- `auto` with trailing return types
- Structured bindings possible but not heavily used
- `std::function` for task callbacks

## Build Requirements

- **C++17 required**: `build_flags = -std=gnu++17`, `build_unflags = -std=gnu++11`
- **Include path**: `-Iinclude` in build flags
- **Libraries**: Exact versions in `platformio.ini` lib_deps (Adafruit, INA226_WE, etc.)

## Memory Constraints

Current usage: 76.2% flash (998KB/1.3MB), 15.4% RAM (50KB/327KB)

Flash-heavy features: ArduinoJson, WiFi/HTTPS, sensor libraries. Optimize if adding features:
- Reduce debug level (`CORE_DEBUG_LEVEL=1` instead of 3)
- Disable unused sensors
- Use `PROGMEM` for large constant strings

## Hardware Notes

- **Motor PWM**: 20kHz at 10-bit resolution to avoid audible noise
- **I2C**: All buses 100kHz. Onboard bus (pins 1,2) has ADS1115 and INA226
- **OneWire**: 4 independent buses for DS18B20 sensors, 12-bit resolution
- **Thermistors**: 10kΩ NTC with 10kΩ series resistor, Steinhart-Hart with β=3950
- **Watchdog**: 5 second hardware timeout, must feed in main loop

## Development Workflow

1. **Edit Config.hpp** with WiFi/MQTT/API settings
2. **Build**: `pio run` - verify compilation
3. **First upload**: USB via `pio run --target upload`
4. **Verify boot**: `pio device monitor` - check WiFi connection, sensor detection
5. **Subsequent uploads**: WiFi OTA via `--upload-port` flag
6. **Test MQTT**: Publish to `home/fan1/power`, observe LED and motor response

## Debugging

Serial output shows:
- Boot sequence with chip ID and firmware version
- Hardware initialization status (each sensor/component)
- WiFi scan results and connection
- Periodic task execution (can add debug prints in task lambdas)

**OTA debugging**: ArduinoOTA prints progress to serial. For HTTPS OTA failures, check `OtaManager.hpp` response handling.
