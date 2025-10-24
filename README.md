# ESP32-S3 Air Quality Controller

Modern C++ embedded firmware for ESP32-S3 based environmental monitoring and fan control system.

## Features

- **Multi-sensor Support**: BME688, Si7021, ZMOD4510, OneWire temperature sensors
- **Motor Control**: PWM-based motor control with fault detection and MQTT integration
- **Power Monitoring**: INA226 for input power measurement
- **Health Monitoring**: ADC-based thermistor readings for motor and MCU temperature
- **WiFi**: Multi-SSID auto-selection based on signal strength
- **MQTT**: Remote motor control via MQTT commands
- **Telemetry**: HTTPS-based data reporting to InfluxDB
- **OTA Updates**: Both ArduinoOTA and HTTPS firmware updates
- **Status LEDs**: Visual feedback for system state

## Architecture

The firmware follows modern C++ embedded practices with a layered architecture:

```
Application Layer (Business Logic & State Machine)
    ↓
Services Layer (WiFi, MQTT, HTTPS, OTA, Telemetry)
    ↓
HAL Layer (Hardware Abstraction)
    ↓
Platform (ESP32 Arduino Framework)
```

### Key Design Principles

- **Type Safety**: Strong typing, enums, Result<T,E> for error handling
- **Memory Safety**: RAII, smart pointers, fixed-size buffers
- **Zero-Cost Abstractions**: C++17 features without runtime overhead
- **Non-Blocking**: Task scheduler for periodic operations
- **Testable**: Interface-based design with dependency injection

## Project Structure

```
├── include/
│   ├── Config.hpp              # System configuration
│   ├── app/
│   │   ├── Application.hpp     # Main application class
│   │   ├── Tasks.hpp          # Task scheduler
│   │   └── Types.hpp          # Common types
│   ├── hal/                   # Hardware Abstraction Layer
│   │   ├── IAdc.hpp
│   │   ├── Ads1115.hpp
│   │   ├── Ina226.hpp
│   │   ├── MotorController.hpp
│   │   ├── LedController.hpp
│   │   ├── I2cBus.hpp
│   │   ├── OneWireBus.hpp
│   │   └── sensors/
│   │       ├── ISensor.hpp
│   │       ├── Bme688.hpp
│   │       ├── Si7021.hpp
│   │       └── Zmod4510.hpp
│   ├── services/              # Service Layer
│   │   ├── WiFiManager.hpp
│   │   ├── MqttClient.hpp
│   │   ├── HttpsClient.hpp
│   │   ├── OtaManager.hpp
│   │   ├── TelemetryService.hpp
│   │   └── WatchdogService.hpp
│   └── util/                  # Utilities
│       ├── Result.hpp
│       ├── Timer.hpp
│       ├── RingBuffer.hpp
│       └── Thermistor.hpp
├── src/
│   └── main.cpp
└── platformio.ini
```

## Hardware Configuration

### Pins

- **LEDs**: Green(4), Orange(5), Red(6), Blue(7)
- **OneWire Buses**: 3, 46, 9, 10
- **External I2C Buses**:
  - Bus 1: SDA(11), SCL(12)
  - Bus 2: SDA(13), SCL(14)
  - Bus 3: SDA(21), SCL(47)
  - Bus 4: SDA(48), SCL(45)
- **Onboard I2C**: SDA(1), SCL(2)
- **Motor**: IN_A(41), IN_B(35), EN_A(40), EN_B(36), PWM(38)

### Onboard Sensors

- **ADS1115** (0x48): 16-bit ADC for thermistors and voltage monitoring
- **INA226** (0x40): Power monitor with 1mΩ shunt

## Building

```bash
# Build firmware
pio run

# Upload firmware
pio run --target upload

# Monitor serial output
pio device monitor
```

## Configuration

Edit `include/Config.hpp` to configure:
- WiFi credentials
- MQTT broker address
- API endpoints
- Pin assignments
- Timing intervals

## Usage

### MQTT Control

Publish to `home/fan1/power` with values 0.0-1.0 to control motor speed.

### Telemetry

Data is automatically sent to the configured InfluxDB endpoint:
- Health reports every 5 seconds
- Sensor readings every 5 seconds
- MQTT status every 10 seconds

### OTA Updates

- **ArduinoOTA**: Enabled by default on the network
- **HTTPS OTA**: Checks for updates hourly at configured endpoint

## Dependencies

All dependencies are managed via PlatformIO:
- Adafruit ADS1X15
- Adafruit BME680 Library
- Adafruit Si7021 Library
- INA226_WE
- PubSubClient (MQTT)
- OneWire
- DallasTemperature
- ArduinoJson

## Memory Usage

```
RAM:   15.4% (50,620 / 327,680 bytes)
Flash: 76.2% (998,929 / 1,310,720 bytes)
```

## License

[Your License Here]

## Notes

- Firmware version is defined in platformio.ini as `FIRMWARE_VERSION`
- Watchdog timeout is 5 seconds
- All I2C buses run at 100kHz
- Motor PWM frequency is 20kHz at 10-bit resolution
- ZMOD4510 sensor driver is a placeholder and requires specific library implementation
