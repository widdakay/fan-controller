#include "app/Application.hpp"
#include "app/Application.hpp"

namespace app {

Application::Application()
    : leds_(std::make_unique<hal::LedController>()),
      motor_(std::make_unique<hal::MotorController>(
          config::PIN_MOTOR_IN_A, config::PIN_MOTOR_IN_B,
          config::PIN_MOTOR_EN_A, config::PIN_MOTOR_EN_B,
          config::PIN_MOTOR_PWM, config::MOTOR_PWM_FREQ_HZ,
          config::MOTOR_PWM_BITS)),
      oneWireConversionTimer_(config::ONEWIRE_CONVERSION_MS),
      thermistor_(config::THERMISTOR_SERIES_R)
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

    // Initialize shared I2C on onboard pins first
    hal::I2cSwitcher::instance().use(config::PIN_I2C_ONBOARD_SDA, config::PIN_I2C_ONBOARD_SCL);

    // Initialize ADS1115
    adc_ = std::make_unique<hal::Ads1115>(hal::I2cSwitcher::instance().wire(), config::I2C_ADDR_ADS1115, 0);
    bool adcOk = adc_->begin();
    Serial.printf("  ADS1115: %s\n", adcOk ? "OK" : "FAILED");

    // Initialize INA226
    powerMonitor_ = std::make_unique<hal::Ina226>(hal::I2cSwitcher::instance().wire(), config::I2C_ADDR_INA226, 0);
    bool inaOk = powerMonitor_->begin(config::INA226_SHUNT_OHM);
    Serial.printf("  INA226: %s\n", inaOk ? "OK" : "FAILED");

    Serial.printf("Onboard sensors: ADS1115=%s, INA226=%s\n", adcOk ? "OK" : "FAIL", inaOk ? "OK" : "FAIL");

    // Initialize motor controller
    motor_->begin();
    motor_->setDirection(false);
    motor_->setPower(0.0f);
    Serial.println("  Motor Controller: OK");

    // Initialize external I2C buses and sensors
    initializeExternalSensors_();

    // Initialize OneWire buses
    initializeOneWire_();
}

void Application::initializeExternalSensors_() {
    Serial.println("Initializing external sensors...");

    // Define I2C bus configurations (external buses only: 1-4)
    struct I2cConfig {
        int sda;
        int scl;
        uint8_t id;
    };

    std::vector<I2cConfig> i2cConfigs;
    for (uint8_t busId = 1; busId <= 4; busId++) {
        auto [sda, scl] = config::getI2CPins(busId);
        if (sda != -1 && scl != -1) {
            i2cConfigs.push_back({sda, scl, busId});
        }
    }

    for (const auto& cfg : i2cConfigs) {
        auto bus = std::make_unique<hal::I2cBus>(cfg.sda, cfg.scl, cfg.id);
        bus->begin();

        Serial.printf("  I2C Bus %d:\n", cfg.id);
        auto devices = bus->scan();

        // Try to initialize sensors on this bus
        size_t cntBme = 0, cntSi = 0, cntZmod = 0, cntUnknown = 0;
        for (uint8_t addr : devices) {
            Serial.printf("    Device at 0x%02X\n", addr);
            if (addr == config::I2C_ADDR_BME688 || addr == 0x77) {
                auto bme = std::make_unique<hal::Bme688>(bus->select(), addr, cfg.id);
                if (bme->begin()) {
                    Serial.printf("    BME688 found at 0x%02X\n", addr);
                    bme688Sensors_.push_back(std::move(bme));
                    cntBme++;
                } else {
                    Serial.printf("    BME688 device at 0x%02X begin() FAILED\n", addr);
                }
            }

            // Si7021 has fixed address 0x40
            if (addr == 0x40) {
                auto si = std::make_unique<hal::Si7021>(bus->select(), cfg.id);
                if (si->begin()) {
                    Serial.printf("    Si7021 found at 0x%02X\n", addr);
                    si7021Sensors_.push_back(std::move(si));
                    cntSi++;
                } else {
                    Serial.printf("    Si7021 device at 0x%02X begin() FAILED\n", addr);
                }
            }

            // AHT20 has fixed address 0x38
            if (addr == config::I2C_ADDR_AHT20) {
                auto aht = std::make_unique<hal::Aht20>(bus->select(), cfg.id, config::I2C_ADDR_AHT20);
                if (aht->begin()) {
                    Serial.printf("    AHT20 found at 0x%02X\n", addr);
                    aht20Sensors_.push_back(std::move(aht));
                    cntSi++;  // Count as Si7021-compatible sensor
                } else {
                    Serial.printf("    AHT20 device at 0x%02X begin() FAILED\n", addr);
                }
            }

            if (addr == config::I2C_ADDR_ZMOD4510) {
                auto zmod = std::make_unique<hal::Zmod4510>(bus->select(), addr, cfg.id);
                if (zmod->begin()) {
                    Serial.printf("    ZMOD4510 found at 0x%02X\n", addr);
                    zmod4510Sensors_.push_back(std::move(zmod));
                    cntZmod++;
                } else {
                    Serial.printf("    ZMOD4510 device at 0x%02X begin() FAILED\n", addr);
                }
            }

            if (addr != config::I2C_ADDR_BME688 && addr != 0x77 &&
                addr != 0x40 && addr != config::I2C_ADDR_AHT20 &&
                addr != config::I2C_ADDR_ZMOD4510) {
                Serial.printf("    Unknown device at 0x%02X\n", addr);
                cntUnknown++;
            }
        }

        Serial.printf("  Bus %d summary: BME688=%zu, Si7021=%zu, ZMOD4510=%zu, Unknown=%zu\n",
                      cfg.id, cntBme, cntSi, cntZmod, cntUnknown);

        i2cBuses_.push_back(std::move(bus));
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
    ota_ = std::make_unique<services::OtaManager>(*https_);
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

app::HardwareConfig Application::buildHardwareConfig_() const {
    app::HardwareConfig config;
    config.chipId = ESP.getEfuseMac();
    config.firmwareVersion = FIRMWARE_VERSION;

    // Onboard hardware status
    config.ads1115Initialized = (adc_ != nullptr);
    config.ina226Initialized = (powerMonitor_ != nullptr);
    config.motorControllerInitialized = (motor_ != nullptr);

    // I2C buses
    for (const auto& bus : i2cBuses_) {
        app::I2cBusInfo busInfo;
        busInfo.busId = bus->getId();
        auto [sda, scl] = config::getI2CPins(bus->getId());
        busInfo.sdaPin = sda;
        busInfo.sclPin = scl;

        // Add sensors for this bus
        // BME688 sensors
        for (const auto& sensor : bme688Sensors_) {
            if (sensor->getBusId() == bus->getId()) {
                busInfo.sensors.push_back({config::I2C_ADDR_BME688, "BME688", true});
            }
        }
        // Si7021 sensors
        for (const auto& sensor : si7021Sensors_) {
            if (sensor->getBusId() == bus->getId()) {
                busInfo.sensors.push_back({0x40, "Si7021", true});
            }
        }
        // AHT20 sensors
        for (const auto& sensor : aht20Sensors_) {
            if (sensor->getBusId() == bus->getId()) {
                busInfo.sensors.push_back({config::I2C_ADDR_AHT20, "AHT20", true});
            }
        }
        // ZMOD4510 sensors
        for (const auto& sensor : zmod4510Sensors_) {
            if (sensor->getBusId() == bus->getId()) {
                busInfo.sensors.push_back({config::I2C_ADDR_ZMOD4510, "ZMOD4510", true});
            }
        }

        config.i2cBuses.push_back(busInfo);
    }

    // OneWire buses
    for (const auto& bus : oneWireBuses_) {
        app::OneWireBusInfo busInfo;
        busInfo.busId = bus->getId();
        busInfo.pin = bus->getPin();
        busInfo.deviceCount = bus->getDeviceCount();

        // Add sensors (DS18B20 devices)
        auto addresses = bus->getDeviceAddresses();
        for (uint64_t addr : addresses) {
            busInfo.sensors.push_back({addr, true});
        }

        config.oneWireBuses.push_back(busInfo);
    }

    return config;
}

void Application::sendBootReportAfterInit_() {
    if (wifi_ && telemetry_) {
        auto hwConfig = buildHardwareConfig_();
        telemetry_->sendBootInfo(bootInfo_, wifi_->getLastScan(), hwConfig);
        telemetry_->flushBatch();
    }
}

void Application::sendHealthReport_() {
    HealthData health;
    health.uptimeMs = millis();

    // Read ADC
    if (adc_) {
        auto adcData = adc_->readAll();
        if (adcData.valid) {
            // Motor temperature
            health.motorTemp.voltage = adcData.motorNtcVolts;
            health.motorTemp.resistance = thermistor_.resistanceFromV(
                adcData.motorNtcVolts, adcData.rail3v3Volts);
            health.motorTemp.tempC = thermistor_.tempC_from_R(health.motorTemp.resistance);
            health.motorTemp.inRange = thermistor_.isValidRange(health.motorTemp.tempC, 0.0f, 100.0f);

            // MCU temperature
            health.mcuExternalTemp.voltage = adcData.mcuNtcVolts;
            health.mcuExternalTemp.resistance = thermistor_.resistanceFromV(
                adcData.mcuNtcVolts, adcData.rail3v3Volts);
            health.mcuExternalTemp.tempC = thermistor_.tempC_from_R(health.mcuExternalTemp.resistance);
            health.mcuExternalTemp.inRange = thermistor_.isValidRange(health.mcuExternalTemp.tempC, 0.0f, 100.0f);

            health.rail3v3 = adcData.rail3v3Volts;
            health.rail5v = adcData.rail5vVolts;
        }
    }

    // Read power monitor
    if (powerMonitor_) {
        auto powerResult = powerMonitor_->read();
        if (powerResult.isOk()) {
            health.inputPower = powerResult.value();
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
    // Start OneWire temperature conversion
    if (!oneWireConversionStarted_) {
        for (auto& bus : oneWireBuses_) {
            bus->requestTemperatures();
        }
        oneWireConversionStarted_ = true;
        oneWireConversionTimer_.reset();
        return;
    }

    // Check if conversion is complete
    if (oneWireConversionTimer_.hasElapsed()) {
        // Read OneWire temperatures
        for (auto& bus : oneWireBuses_) {
            auto readings = bus->readAll();
            if (telemetry_ && !readings.empty()) {
                telemetry_->sendOneWireData(readings);
            }
        }
        oneWireConversionStarted_ = false;
    }

    // Read environmental sensors
    // BME688
    for (auto& sensor : bme688Sensors_) {
        uint32_t timestamp = millis();
        auto result = sensor->read();
        if (result.isOk() && telemetry_) {
            auto reading = result.value();
            Serial.printf("[%lu] BME688 bus %u: T=%.2fC RH=%.2f%% P=%.0fPa Gas=%.0f\n",
                         timestamp, sensor->getBusId(), reading.tempC, reading.humidity,
                         reading.pressurePa, reading.gasResistance);
            
            StaticJsonDocument<256> doc;
            JsonObject fields = doc.to<JsonObject>();
            fields["temp_c"] = reading.tempC;
            fields["humidity"] = reading.humidity;
            fields["pressure_pa"] = reading.pressurePa;
            fields["gas_resistance"] = reading.gasResistance;

            Serial.printf("[%lu] BME688 bus %u: created fields object with %zu fields\n",
                         millis(), sensor->getBusId(), fields.size());
            
            telemetry_->sendSensorData("bme688", sensor->getBusId(), fields);
        } else if (!result.isOk()) {
            Serial.printf("[%lu] BME688 bus %u: read failed\n", timestamp, sensor->getBusId());
        }
    }

    // Si7021
    for (auto& sensor : si7021Sensors_) {
        auto result = sensor->read();
        if (result.isOk() && telemetry_) {
            auto reading = result.value();
            StaticJsonDocument<256> doc;
            JsonObject fields = doc.to<JsonObject>();
            fields["temp_c"] = reading.tempC;
            fields["humidity"] = reading.humidity;

            telemetry_->sendSensorData("si7021", sensor->getBusId(), fields,
                                      reading.serialNumber);
        }
    }

    // AHT20
    for (auto& sensor : aht20Sensors_) {
        uint32_t timestamp = millis();
        auto result = sensor->read();
        if (result.isOk() && telemetry_) {
            auto reading = result.value();
            Serial.printf("[%lu] AHT20 bus %u: T=%.2fC RH=%.2f%%\n",
                         timestamp, sensor->getBusId(), reading.tempC, reading.humidity);
            
            StaticJsonDocument<256> doc;
            JsonObject fields = doc.to<JsonObject>();
            fields["temp_c"] = reading.tempC;
            fields["humidity"] = reading.humidity;

            Serial.printf("[%lu] AHT20 bus %u: created fields object with %zu fields\n",
                         millis(), sensor->getBusId(), fields.size());

            telemetry_->sendSensorData("aht20", sensor->getBusId(), fields,
                                      reading.serialNumber);
        } else if (!result.isOk()) {
            Serial.printf("[%lu] AHT20 bus %u: read failed\n", timestamp, sensor->getBusId());
        }
    }

    // ZMOD4510
    for (auto& sensor : zmod4510Sensors_) {
        auto result = sensor->read();
        if (result.isOk() && telemetry_) {
            auto reading = result.value();
            if (reading.valid) {
                StaticJsonDocument<256> doc;
                JsonObject fields = doc.to<JsonObject>();
                if (std::isfinite(reading.tempC)) {
                    fields["temp_c"] = reading.tempC;
                }
                if (std::isfinite(reading.humidity)) {
                    fields["humidity"] = reading.humidity;
                }
                if (std::isfinite(reading.aqi)) {
                    fields["aqi"] = reading.aqi;
                }
                if (std::isfinite(reading.ozonePpb)) {
                    fields["ozone_ppb"] = reading.ozonePpb;
                }
                if (std::isfinite(reading.no2Ppb)) {
                    fields["no2_ppb"] = reading.no2Ppb;
                }

                telemetry_->sendSensorData("zmod4510", sensor->getBusId(), fields);
            }
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
