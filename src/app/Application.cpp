#include "app/Application.hpp"
#include <map>
#include <ArduinoJson.h>
#include "util/Logger.hpp"

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
    Logger::begin(115200);
    delay(500);
    Logger::info("=== ESP32 Air Quality Controller ===");
    Logger::info("Firmware: %s", FIRMWARE_VERSION);
    Logger::info("Chip ID: %llx", ESP.getEfuseMac());

    // Initialize configuration manager (must be first!)
    auto configResult = config_.begin();
    if (configResult.isErr()) {
        Logger::error("FATAL: Failed to initialize configuration!");
        while (1) { delay(1000); }  // Halt
    }

    // Print current configuration
    config_.printConfig();

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

    Logger::info("=== Initialization Complete ===");
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
    Logger::info("Initializing hardware...");

    // Initialize sensor registry
    hal::initializeSensorRegistry();
    hal::SensorRegistry::instance().printRegistry();

    // Initialize motor controller
    motor_->begin();
    motor_->setDirection(false);
    motor_->setPower(0.0f);
    Logger::info("  Motor Controller: OK");

    // Discover all I2C sensors on all buses (including onboard bus 0)
    discoverAllSensors_();

    // Initialize OneWire buses
    initializeOneWire_();
}

void Application::discoverAllSensors_() {
    Logger::info("Discovering sensors on all I2C buses...");

    // Discover on ALL I2C buses (0-4), treating them equally
    // Bus 0 is the "onboard" bus, buses 1-4 are "external"
    for (uint8_t busId = 0; busId <= 4; busId++) {
        auto [sda, scl] = config::getI2CPins(busId);
        if (sda == -1 || scl == -1) {
            continue; // Skip unconfigured buses
        }

        Logger::info("=== I2C Bus %d (SDA=%d, SCL=%d) ===", busId, sda, scl);

        // Create bus and scan for devices
        auto bus = std::make_unique<hal::I2cBus>(sda, scl, busId);
        if (!bus->begin()) {
            Logger::error("  Bus %d initialization FAILED", busId);
            continue;
        }

        auto devices = bus->scan();
        if (devices.empty()) {
            Logger::info("  No devices found on bus %d", busId);
            continue;
        }

        Logger::info("  Found %zu device(s):", devices.size());

        // For each device address, try to match with registered sensors
        for (uint8_t addr : devices) {
            Logger::info("    0x%02X: ", addr);

            auto descriptors = hal::SensorRegistry::instance().findByAddress(addr);

            if (descriptors.empty()) {
                Logger::info("unknown device");
                continue;
            }

            // Try each matching descriptor until one succeeds
            bool initialized = false;
            for (const auto* desc : descriptors) {
                Logger::info("trying %s... ", desc->typeName);

                auto sensor = desc->factory(*bus, addr);
                if (sensor) {
                    Logger::info("OK");

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
                    Logger::info("failed, ");
                }
            }

            if (!initialized) {
                Logger::info("all attempts failed");
            }
        }
    }

    Logger::info("=== Sensor Discovery Complete ===");
    Logger::info("Total sensors discovered: %zu", sensors_.size());

    // Print summary by type
    std::map<String, int> typeCounts;
    for (const auto& sensor : sensors_) {
        String type = sensor->getTypeName();
        typeCounts[type]++;
    }

    Logger::info("Sensor types:");
    for (const auto& [type, count] : typeCounts) {
        Logger::info("  %s: %d", type.c_str(), count);
    }
}

void Application::initializeOneWire_() {
    Logger::info("Initializing OneWire buses...");

    std::vector<std::pair<int, uint8_t>> oneWireConfigs = {
        {config::PIN_ONEWIRE_1, 0},
        {config::PIN_ONEWIRE_2, 1},
        {config::PIN_ONEWIRE_3, 2},
        {config::PIN_ONEWIRE_4, 3}
    };

    for (const auto& cfg : oneWireConfigs) {
        auto bus = std::make_unique<hal::OneWireBus>(cfg.first, cfg.second);
        if (bus->begin() && bus->getDeviceCount() > 0) {
            Logger::info("  OneWire Bus %d: %d device(s)",
                          cfg.second, bus->getDeviceCount());
            oneWireBuses_.push_back(std::move(bus));
        }
    }
}

void Application::connectWiFi_() {
    Logger::info("Connecting to WiFi...");

    wifi_ = std::make_unique<services::WiFiManager>();
    auto result = wifi_->connect(config_.get().wifiCredentials);

    if (result.isOk()) {
        Logger::info("Connected to: %s", wifi_->getConnectedSSID().c_str());
        Logger::info("IP Address: %s", wifi_->getLocalIP().c_str());
        Logger::info("RSSI: %d dBm", wifi_->getRSSI());
    } else {
        Logger::error("WiFi connection failed!");
        leds_->errorFlash();
    }
}

void Application::initializeServices_() {
    Logger::info("Initializing services...");

    // Initialize HTTPS client
    https_ = std::make_unique<services::HttpsClient>();

    // Initialize MQTT
    mqtt_ = std::make_unique<services::MqttClient>(wifiClient_);
    const auto& cfg = config_.get();
    mqtt_->begin(cfg.mqttServer, cfg.mqttPort,
                  cfg.mqttTopicPowerCommand, cfg.mqttTopicPowerStatus);
    mqtt_->setMessageCallback([this](const char* topic, float value) {
        this->handleMqttMessage_(topic, value);
    });
    mqtt_->setConfigCallback([this](const char* topic, const char* payload) {
        this->handleConfigMessage_(topic, payload);
    });

    // Enable MQTT logging (send INFO level and above to MQTT)
    Logger::enableMqttLogging(true);
    Logger::setMqttLogLevel(LogLevel::INFO);
    Logger::setMqttLogTopic(String(cfg.deviceName) + "/logs");
    Logger::setMqttCallback([this](const char* topic, const String& payload) -> bool {
        if (mqtt_ && mqtt_->isConnected()) {
            return mqtt_->publish(topic, payload, false);
        }
        return false;
    });

    // Initialize OTA
    ota_ = std::make_unique<services::OtaManager>(*https_, watchdog_);
    ota_->begin(cfg.deviceName, cfg.apiFirmwareUpdate);
    ota_->setOtaCallback([this](bool active) {
        leds_->setOtaStatus(active);
    });

    // Initialize telemetry
    telemetry_ = std::make_unique<services::TelemetryService>(*https_,
                                                                cfg.deviceName,
                                                                cfg.apiInfluxDb);

    // Send boot report now that services are initialized
    sendBootReportAfterInit_();

    Logger::info("Services initialized");
}

void Application::registerTasks_() {
    Logger::info("Registering tasks...");

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

    Logger::info("Registered %zu tasks", scheduler_.taskCount());
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
    Logger::info("[%lu] Reading %zu sensors...", timestamp, sensors_.size());

    for (const auto& sensor : sensors_) {
        if (!sensor->isConnected()) {
            Logger::warn("[%lu] %s on bus %u: disconnected",
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

                Logger::debug("[%lu] %s bus %u: %s",
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
                Logger::error("[%lu] %s bus %u: JSON parse failed: %s",
                              timestamp, sensor->getTypeName(), sensor->getBusId(),
                              error.c_str());
            }
        } else if (!jsonResult.isOk()) {
            Logger::error("[%lu] %s bus %u: read failed",
                          timestamp, sensor->getTypeName(), sensor->getBusId());
        }
    }

    // Flush batch after collecting all sensor readings
    if (telemetry_) {
        Logger::debug("[%lu] readAndReportSensors_: calling flushBatch", millis());
        telemetry_->flushBatch();
    }
}

void Application::handleMqttMessage_(const char* topic, float value) {
    Logger::info("Handling MQTT: %s = %.3f", topic, value);

    const auto& cfg = config_.get();
    if (strcmp(topic, cfg.mqttTopicPowerCommand.c_str()) == 0) {
        if (motor_) {
            motor_->setFromMqtt(value);
            Logger::info("Motor power set to: %.1f%%", value * 100.0f);
        }
    }
}

void Application::handleConfigMessage_(const char* topic, const char* payload) {
    Logger::info("Config command: %s", payload);

    // Parse JSON command
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
        Logger::error("Config JSON parse error: %s", error.c_str());
        return;
    }

    const char* cmd = doc["cmd"];
    if (!cmd) {
        Logger::error("Missing 'cmd' field in config message");
        return;
    }

    // Handle different configuration commands
    if (strcmp(cmd, "set_device_name") == 0) {
        const char* name = doc["name"];
        if (name) {
            auto result = config_.setDeviceName(String(name));
            if (result.isOk()) {
                Logger::info("Device name set to: %s", name);
                mqtt_->publish((String(topic) + "/status").c_str(), "OK: Device name updated", false);
            } else {
                mqtt_->publish((String(topic) + "/status").c_str(), "ERROR: Invalid device name", false);
            }
        }
    }
    else if (strcmp(cmd, "set_mqtt_server") == 0) {
        const char* server = doc["server"];
        uint16_t port = doc["port"] | 1883;
        if (server) {
            auto result = config_.setMqttServer(String(server), port);
            if (result.isOk()) {
                Logger::info("MQTT server set to: %s:%d (restart required)", server, port);
                mqtt_->publish((String(topic) + "/status").c_str(), "OK: MQTT server updated, restart required", false);
            } else {
                mqtt_->publish((String(topic) + "/status").c_str(), "ERROR: Invalid MQTT server", false);
            }
        }
    }
    else if (strcmp(cmd, "set_wifi") == 0) {
        uint8_t index = doc["index"] | 0;
        const char* ssid = doc["ssid"];
        const char* password = doc["password"];
        if (ssid && password) {
            auto result = config_.setWifiCredential(index, String(ssid), String(password));
            if (result.isOk()) {
                Logger::info("WiFi %d set to: %s (restart required)", index, ssid);
                mqtt_->publish((String(topic) + "/status").c_str(), "OK: WiFi updated, restart required", false);
            } else {
                mqtt_->publish((String(topic) + "/status").c_str(), "ERROR: Invalid WiFi credentials", false);
            }
        }
    }
    else if (strcmp(cmd, "set_mqtt_topics") == 0) {
        const char* cmdTopic = doc["command"];
        const char* statTopic = doc["status"];
        if (cmdTopic && statTopic) {
            auto result = config_.setMqttTopics(String(cmdTopic), String(statTopic));
            if (result.isOk()) {
                Logger::info("MQTT topics updated (restart required)");
                mqtt_->publish((String(topic) + "/status").c_str(), "OK: Topics updated, restart required", false);
            } else {
                mqtt_->publish((String(topic) + "/status").c_str(), "ERROR: Invalid topics", false);
            }
        }
    }
    else if (strcmp(cmd, "set_api_endpoints") == 0) {
        const char* influx = doc["influxdb"];
        const char* fwUpdate = doc["firmware"];
        if (influx && fwUpdate) {
            auto result = config_.setApiEndpoints(String(influx), String(fwUpdate));
            if (result.isOk()) {
                Logger::info("API endpoints updated (restart required)");
                mqtt_->publish((String(topic) + "/status").c_str(), "OK: API endpoints updated, restart required", false);
            } else {
                mqtt_->publish((String(topic) + "/status").c_str(), "ERROR: Invalid endpoints", false);
            }
        }
    }
    else if (strcmp(cmd, "print_config") == 0) {
        config_.printConfig();
        mqtt_->publish((String(topic) + "/status").c_str(), "OK: Config printed to serial", false);
    }
    else if (strcmp(cmd, "reset_config") == 0) {
        auto result = config_.resetToDefaults();
        if (result.isOk()) {
            Logger::info("Config reset to defaults (restart required)");
            mqtt_->publish((String(topic) + "/status").c_str(), "OK: Config reset, restart required", false);
        } else {
            mqtt_->publish((String(topic) + "/status").c_str(), "ERROR: Reset failed", false);
        }
    }
    else {
        Logger::error("Unknown config command: %s", cmd);
        mqtt_->publish((String(topic) + "/status").c_str(), "ERROR: Unknown command", false);
    }
}

} // namespace app
