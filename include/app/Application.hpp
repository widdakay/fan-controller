#pragma once
#include "Config.hpp"
#include "Tasks.hpp"
#include "Types.hpp"
#include "hal/Ads1115.hpp"
#include "hal/Ina226.hpp"
#include "hal/MotorController.hpp"
#include "hal/LedController.hpp"
#include "hal/I2cBus.hpp"
#include "hal/OneWireBus.hpp"
#include "hal/sensors/Bme688.hpp"
#include "hal/sensors/Si7021.hpp"
#include "hal/sensors/Aht20.hpp"
#include "hal/sensors/Zmod4510.hpp"
#include "services/WiFiManager.hpp"
#include "services/MqttClient.hpp"
#include "services/HttpsClient.hpp"
#include "services/OtaManager.hpp"
#include "services/TelemetryService.hpp"
#include "services/WatchdogService.hpp"
#include "util/Thermistor.hpp"
#include "util/Timer.hpp"
#include <memory>
#include <vector>
#include "hal/I2cSwitcher.hpp"
#include <Arduino.h>
#include <Wire.h>
#include <esp_system.h>

namespace app {

class Application {
public:
    Application()
        : leds_(std::make_unique<hal::LedController>()),
          motor_(std::make_unique<hal::MotorController>(
              config::PIN_MOTOR_IN_A, config::PIN_MOTOR_IN_B,
              config::PIN_MOTOR_EN_A, config::PIN_MOTOR_EN_B,
              config::PIN_MOTOR_PWM, config::MOTOR_PWM_FREQ_HZ,
              config::MOTOR_PWM_BITS)),
          oneWireConversionTimer_(config::ONEWIRE_CONVERSION_MS),
          thermistor_(config::THERMISTOR_SERIES_R)
    {}

    void setup() {
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

    void loop() {
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

private:
    // ========================================================================
    // Hardware Components
    // ========================================================================
    std::unique_ptr<hal::LedController> leds_;
    std::unique_ptr<hal::Ads1115> adc_;
    std::unique_ptr<hal::Ina226> powerMonitor_;
    std::unique_ptr<hal::MotorController> motor_;

    // I2C buses (external sensors)
    std::vector<std::unique_ptr<hal::I2cBus>> i2cBuses_;
    std::vector<std::unique_ptr<hal::Bme688>> bme688Sensors_;
    std::vector<std::unique_ptr<hal::Si7021>> si7021Sensors_;
    std::vector<std::unique_ptr<hal::Aht20>> aht20Sensors_;
    std::vector<std::unique_ptr<hal::Zmod4510>> zmod4510Sensors_;

    // OneWire buses
    std::vector<std::unique_ptr<hal::OneWireBus>> oneWireBuses_;
    util::Timer oneWireConversionTimer_;
    bool oneWireConversionStarted_ = false;

    // ========================================================================
    // Services
    // ========================================================================
    services::WatchdogService watchdog_;
    std::unique_ptr<services::WiFiManager> wifi_;
    WiFiClient wifiClient_;
    std::unique_ptr<services::MqttClient> mqtt_;
    std::unique_ptr<services::HttpsClient> https_;
    std::unique_ptr<services::OtaManager> ota_;
    std::unique_ptr<services::TelemetryService> telemetry_;

    // ========================================================================
    // Task Scheduler
    // ========================================================================
    TaskScheduler scheduler_;

    // ========================================================================
    // Utilities
    // ========================================================================
    util::Thermistor thermistor_;

    // ========================================================================
    // Initialization Methods
    // ========================================================================

    void initializeHardware_() {
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

    void initializeExternalSensors_() {
        Serial.println("Initializing external sensors...");

        // Define I2C bus configurations
        struct I2cConfig {
            int sda;
            int scl;
            uint8_t id;
        };

        std::vector<I2cConfig> i2cConfigs = {
            {config::PIN_I2C1_SDA, config::PIN_I2C1_SCL, 1},
            {config::PIN_I2C2_SDA, config::PIN_I2C2_SCL, 2},
            {config::PIN_I2C3_SDA, config::PIN_I2C3_SCL, 3},
            {config::PIN_I2C4_SDA, config::PIN_I2C4_SCL, 4}
        };

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

    void initializeOneWire_() {
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

    void connectWiFi_() {
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

    void initializeServices_() {
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

        Serial.println("Services initialized");
    }

    void registerTasks_() {
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

    // ========================================================================
    // Task Handlers
    // ========================================================================

    void sendBootReport_() {
        BootInfo boot;
        boot.chipId = ESP.getEfuseMac();
        boot.resetReason = esp_reset_reason() == ESP_RST_POWERON ? "PowerOn" : "Other";
        boot.sketchSize = ESP.getSketchSize();
        boot.freeSketchSpace = ESP.getFreeSketchSpace();
        boot.heapSize = ESP.getHeapSize();
        boot.firmwareVersion = FIRMWARE_VERSION;

        // Note: WiFi scan results will be captured during connect
        if (wifi_) {
            telemetry_->sendBootInfo(boot, wifi_->getLastScan());
        }
    }

    void sendHealthReport_() {
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

        // Send telemetry
        if (telemetry_) {
            telemetry_->sendHealthReport(health);
        }
    }

    void publishMqttStatus_() {
        if (mqtt_ && mqtt_->isConnected() && motor_) {
            float powerLevel = motor_->getPowerLevel();
            mqtt_->publishPowerStatus(powerLevel);
        }
    }

    void readAndReportSensors_() {
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
            auto result = sensor->read();
            if (result.isOk() && telemetry_) {
                auto reading = result.value();
                StaticJsonDocument<256> doc;
                JsonObject fields = doc.to<JsonObject>();
                fields["temp_c"] = reading.tempC;
                fields["humidity"] = reading.humidity;
                fields["pressure_pa"] = reading.pressurePa;
                fields["gas_resistance"] = reading.gasResistance;

                telemetry_->sendSensorData("bme688", sensor->getBusId(), fields);
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
    }

    void handleMqttMessage_(const char* topic, float value) {
        Serial.printf("Handling MQTT: %s = %.3f\n", topic, value);

        if (strcmp(topic, config::MQTT_TOPIC_POWER_COMMAND) == 0) {
            if (motor_) {
                motor_->setFromMqtt(value);
                Serial.printf("Motor power set to: %.1f%%\n", value * 100.0f);
            }
        }
    }
};

} // namespace app
