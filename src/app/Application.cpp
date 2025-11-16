#include "app/Application.hpp"
#include <map>
#include <ArduinoJson.h>

namespace app {

Application::Application()
    : leds_(std::make_unique<hal::LedController>()),
      motor_(std::make_unique<hal::MotorController>(
          config::PIN_MOTOR_IN_A, config::PIN_MOTOR_IN_B,
          config::PIN_MOTOR_EN_A, config::PIN_MOTOR_EN_B,
          config::PIN_MOTOR_PWM, config::MOTOR_PWM_FREQ_HZ,
          config::MOTOR_PWM_BITS)),
      oneWireConversionTimer_(config::ONEWIRE_CONVERSION_MS)
{}

void Application::setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n\n=== ESP32 Air Quality Controller ===");
    Serial.printf("Firmware: %s\n", FIRMWARE_VERSION);
    Serial.printf("Chip ID: %llx\n", ESP.getEfuseMac());

    // Initialize watchdog
    watchdog_.begin();

    // Initialize LEDs
    leds_->allOff();
    leds_->heartbeat();

    // Send boot report
    sendBootReport_();

    // Initialize hardware
    initializeHardware_();

    // Connect to WiFi
    connectWiFi_();

    // Initialize services
    initializeServices_();

    // Register tasks
    registerTasks_();

    Serial.println("=== Initialization Complete ===\n");
}

void Application::loop() {
    // Feed watchdog
    watchdog_.feed();

    // Update LEDs
    leds_->update();

    // Handle OTA
    ota_->handle();

    // Handle MQTT
    mqtt_->loop();

    // Run scheduled tasks
    scheduler_.tick();

    // Small yield
    delay(1);
}

void Application::initializeHardware_() {
    Serial.println("Initializing hardware...");

    // Initialize sensor registry
    hal::initializeSensorRegistry();
    hal::SensorRegistry::instance().printRegistry();

    // Initialize motor controller
    motor_->begin();
    motor_->setDirection(false);
    motor_->setPower(0.0f);
    Serial.println("  Motor Controller: OK");

    // Discover all I2C sensors on all buses (including onboard bus 0)
    discoverAllSensors_();

    // Initialize OneWire buses
    initializeOneWire_();
}

void Application::discoverAllSensors_() {
    Serial.println("Discovering sensors on all I2C buses...");

    // Discover on ALL I2C buses (0-4), treating them equally
    // Bus 0 is the "onboard" bus, buses 1-4 are "external"
    for (uint8_t busId = 0; busId <= 4; busId++) {
        auto [sda, scl] = config::getI2CPins(busId);
        if (sda == -1 || scl == -1) {
            continue; // Skip unconfigured buses
        }

        Serial.printf("\n=== I2C Bus %d (SDA=%d, SCL=%d) ===\n", busId, sda, scl);

        // Create bus and scan for devices
        auto bus = std::make_unique<hal::I2cBus>(sda, scl, busId);
        if (!bus->begin()) {
            Serial.printf("  Bus %d initialization FAILED\n", busId);
            continue;
        }

        auto devices = bus->scan();
        if (devices.empty()) {
            Serial.printf("  No devices found on bus %d\n", busId);
            continue;
        }

        Serial.printf("  Found %zu device(s):\n", devices.size());

        // For each device address, try to match with registered sensors
        for (uint8_t addr : devices) {
            Serial.printf("    0x%02X: ", addr);

            auto descriptors = hal::SensorRegistry::instance().findByAddress(addr);

            if (descriptors.empty()) {
                Serial.println("unknown device");
                continue;
            }

            // Try each matching descriptor until one succeeds
            bool initialized = false;
            for (const auto* desc : descriptors) {
                Serial.printf("trying %s... ", desc->typeName);

                auto sensor = desc->factory(*bus, addr);
                if (sensor) {
                    Serial.printf("OK\n");

                    // Check if sensor needs post-processing (e.g., ADC creates thermistors)
                    if (sensor->needsPostProcessing()) {
                        auto virtualSensors = sensor->createPostProcessedSensors();
                        for (auto& vs : virtualSensors) {
                            sensors_.push_back(std::move(vs));
                        }
                    }

                    sensors_.push_back(std::move(sensor));
                    initialized = true;
                    break;
                } else {
                    Serial.printf("failed, ");
                }
            }

            if (!initialized) {
                Serial.println("all attempts failed");
            }
        }
    }

    Serial.printf("\n=== Sensor Discovery Complete ===\n");
    Serial.printf("Total sensors discovered: %zu\n", sensors_.size());

    // Print summary by type
    std::map<String, int> typeCounts;
    for (const auto& sensor : sensors_) {
        String type = sensor->getTypeName();
        typeCounts[type]++;
    }

    Serial.println("Sensor types:");
    for (const auto& [type, count] : typeCounts) {
        Serial.printf("  %s: %d\n", type.c_str(), count);
    }
}

void Application::initializeOneWire_() {
    Serial.println("Initializing OneWire buses...");

    std::vector<std::pair<int, uint8_t>> oneWireConfigs = {
        {config::PIN_ONEWIRE_1, 0},
        {config::PIN_ONEWIRE_2, 1},
        {config::PIN_ONEWIRE_3, 2},
        {config::PIN_ONEWIRE_4, 3}
    };

    for (const auto& cfg : oneWireConfigs) {
        auto bus = std::make_unique<hal::OneWireBus>(cfg.first, cfg.second);
        if (bus->begin() && bus->getDeviceCount() > 0) {
            Serial.printf("  OneWire Bus %d: %d device(s)\n",
                         cfg.second, bus->getDeviceCount());
            oneWireBuses_.push_back(std::move(bus));
        }
    }
}

void Application::connectWiFi_() {
    Serial.println("Connecting to WiFi...");

    wifi_ = std::make_unique<services::WiFiManager>();
    auto result = wifi_->connect();

    if (result.isOk()) {
        Serial.printf("Connected to: %s\n", wifi_->getConnectedSSID().c_str());
        Serial.printf("IP Address: %s\n", wifi_->getLocalIP().c_str());
        Serial.printf("RSSI: %d dBm\n", wifi_->getRSSI());
    } else {
        Serial.println("WiFi connection failed!");
        leds_->errorFlash();
    }
}

void Application::initializeServices_() {
    Serial.println("Initializing services...");

    // Initialize HTTPS client
    https_ = std::make_unique<services::HttpsClient>();

    // Initialize MQTT
    mqtt_ = std::make_unique<services::MqttClient>(wifiClient_);
    mqtt_->begin();
    mqtt_->setMessageCallback([this](const char* topic, float value) {
        this->handleMqttMessage_(topic, value);
    });

    // Initialize OTA
    ota_ = std::make_unique<services::OtaManager>(*https_, watchdog_);
    ota_->begin();
    ota_->setOtaCallback([this](bool active) {
        leds_->setOtaStatus(active);
    });

    // Initialize telemetry
    telemetry_ = std::make_unique<services::TelemetryService>(*https_);

    // Send boot report now that services are initialized
    sendBootReportAfterInit_();

    Serial.println("Services initialized");
}

void Application::registerTasks_() {
    Serial.println("Registering tasks...");

    scheduler_.addTask("heartbeat", [this]() {
        leds_->heartbeat();
    }, config::TASK_LED_HEARTBEAT_MS);

    scheduler_.addTask("health_report", [this]() {
        sendHealthReport_();
    }, config::TASK_HEALTH_REPORT_MS);

    scheduler_.addTask("mqtt_publish", [this]() {
        publishMqttStatus_();
    }, config::TASK_MQTT_PUBLISH_MS);

    scheduler_.addTask("sensor_read", [this]() {
        readAndReportSensors_();
    }, config::TASK_SENSOR_READ_MS);

    scheduler_.addTask("fw_check", [this]() {
        ota_->checkForUpdate();
    }, config::TASK_FW_CHECK_MS);

    Serial.printf("Registered %zu tasks\n", scheduler_.taskCount());
}

void Application::sendBootReport_() {
    BootInfo boot;
    boot.chipId = ESP.getEfuseMac();
    boot.resetReason = esp_reset_reason() == ESP_RST_POWERON ? "PowerOn" : "Other";
    boot.sketchSize = ESP.getSketchSize();
    boot.freeSketchSpace = ESP.getFreeSketchSpace();
    boot.heapSize = ESP.getHeapSize();
    boot.firmwareVersion = FIRMWARE_VERSION;

    // Note: WiFi scan results will be captured during connect
    // Boot info is sent after services are initialized (in initializeServices_)
    // Store boot info for later sending
    bootInfo_ = boot;
}

void Application::sendBootReportAfterInit_() {
    if (wifi_ && telemetry_) {
        telemetry_->sendBootInfo(bootInfo_, wifi_->getLastScan());
        telemetry_->flushBatch();
    }
}

void Application::sendHealthReport_() {
    HealthData health;
    health.uptimeMs = millis();

    // Note: ADC thermistor data and voltage rails are now reported as separate
    // sensors by the virtual sensors. The health report focuses on system-level data.

    // Find and read power monitor sensor
    for (const auto& sensor : sensors_) {
        if (strcmp(sensor->getTypeName(), "INA226") == 0) {
            auto jsonResult = sensor->readAsJson();
            if (jsonResult.isOk()) {
                // Parse the JSON to extract power data
                // For now, we'll report it via the sensor telemetry system
                // The health report can reference power sensor data
            }
        }
    }

    // Get motor status
    if (motor_) {
        health.motor = motor_->getStatus();
        leds_->setMotorStatus(health.motor.dutyCycle > 0.01f);
    }

    // System info
    health.freeHeap = ESP.getFreeHeap();
    health.wifiRssi = wifi_ ? wifi_->getRSSI() : -100;
    health.mqttConnected = mqtt_ ? mqtt_->isConnected() : false;

    // Read MCU internal temperature (ESP32 built-in sensor)
    health.mcuInternalTempC = temperatureRead();

    // Send telemetry
    if (telemetry_) {
        telemetry_->sendHealthReport(health);
        telemetry_->flushBatch();
    }
}

void Application::publishMqttStatus_() {
    if (mqtt_ && mqtt_->isConnected() && motor_) {
        float powerLevel = motor_->getPowerLevel();
        mqtt_->publishPowerStatus(powerLevel);
    }
}

void Application::readAndReportSensors_() {
    // Start OneWire temperature conversion if not already started
    if (!oneWireConversionStarted_) {
        for (auto& bus : oneWireBuses_) {
            bus->requestTemperatures();
        }
        oneWireConversionStarted_ = true;
        oneWireConversionTimer_.reset();
    }

    // Read OneWire temperatures only if conversion is complete
    if (oneWireConversionStarted_ && oneWireConversionTimer_.hasElapsed()) {
        for (auto& bus : oneWireBuses_) {
            auto readings = bus->readAll();
            if (telemetry_ && !readings.empty()) {
                telemetry_->sendOneWireData(readings);
            }
        }
        oneWireConversionStarted_ = false;
    }

    // Always continue to read I2C sensors below (removed early return)

    // Read all I2C sensors using unified interface
    uint32_t timestamp = millis();
    Serial.printf("[%lu] Reading %zu sensors...\n", timestamp, sensors_.size());

    for (const auto& sensor : sensors_) {
        if (!sensor->isConnected()) {
            Serial.printf("[%lu] %s on bus %u: disconnected\n",
                         timestamp, sensor->getTypeName(), sensor->getBusId());
            continue;
        }

        auto jsonResult = sensor->readAsJson();
        if (jsonResult.isOk() && telemetry_) {
            String jsonFields = jsonResult.value();

            // Parse JSON string to ArduinoJson object for telemetry
            StaticJsonDocument<512> doc;
            DeserializationError error = deserializeJson(doc, jsonFields);

            if (!error) {
                JsonObject fields = doc.as<JsonObject>();

                Serial.printf("[%lu] %s bus %u: %s\n",
                             timestamp, sensor->getTypeName(), sensor->getBusId(),
                             jsonFields.c_str());

                // Send to telemetry with sensor's measurement name
                auto serial = sensor->getSerial();
                auto sensorNameOpt = sensor->getSensorName();

                // Get sensor name as String (empty if not present)
                String sensorNameStr = sensorNameOpt.has_value() ? sensorNameOpt.value() : String();

                if (serial.has_value()) {
                    telemetry_->sendSensorData(sensor->getMeasurementName(),
                                             sensor->getBusId(), fields, serial.value(), sensorNameStr);
                } else {
                    telemetry_->sendSensorData(sensor->getMeasurementName(),
                                             sensor->getBusId(), fields, 0, sensorNameStr);
                }
            } else {
                Serial.printf("[%lu] %s bus %u: JSON parse failed: %s\n",
                             timestamp, sensor->getTypeName(), sensor->getBusId(),
                             error.c_str());
            }
        } else if (!jsonResult.isOk()) {
            Serial.printf("[%lu] %s bus %u: read failed\n",
                         timestamp, sensor->getTypeName(), sensor->getBusId());
        }
    }

    // Flush batch after collecting all sensor readings
    if (telemetry_) {
        Serial.printf("[%lu] readAndReportSensors_: calling flushBatch\n", millis());
        telemetry_->flushBatch();
    }
}

void Application::handleMqttMessage_(const char* topic, float value) {
    Serial.printf("Handling MQTT: %s = %.3f\n", topic, value);

    if (strcmp(topic, config::MQTT_TOPIC_POWER_COMMAND) == 0) {
        if (motor_) {
            motor_->setFromMqtt(value);
            Serial.printf("Motor power set to: %.1f%%\n", value * 100.0f);
        }
    }
}

} // namespace app
