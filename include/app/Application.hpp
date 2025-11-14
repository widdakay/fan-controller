#pragma once
#include "Config.hpp"
#include "Tasks.hpp"
#include "Types.hpp"
#include "hal/MotorController.hpp"
#include "hal/LedController.hpp"
#include "hal/I2cBus.hpp"
#include "hal/OneWireBus.hpp"
#include "hal/sensors/AllSensors.hpp"
#include "services/WiFiManager.hpp"
#include "services/MqttClient.hpp"
#include "services/HttpsClient.hpp"
#include "services/OtaManager.hpp"
#include "services/TelemetryService.hpp"
#include "services/WatchdogService.hpp"
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
    Application();

    void setup();
    void loop();

private:
    // ========================================================================
    // Hardware Components
    // ========================================================================
    std::unique_ptr<hal::LedController> leds_;
    std::unique_ptr<hal::MotorController> motor_;

    // Unified sensor management
    // All I2C sensors (including ADC, power monitor, environmental sensors)
    // are stored in a single heterogeneous collection
    std::vector<std::unique_ptr<hal::ISensorInstance>> sensors_;

    // OneWire buses (managed separately due to different protocol)
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
    // Stored Data
    // ========================================================================
    BootInfo bootInfo_;

    // ========================================================================
    // Initialization Methods
    // ========================================================================
    void initializeHardware_();
    void discoverAllSensors_();
    void initializeOneWire_();
    void connectWiFi_();
    void initializeServices_();
    void registerTasks_();

    // ========================================================================
    // Task Handlers
    // ========================================================================
    void sendBootReport_();
    void sendBootReportAfterInit_();
    void sendHealthReport_();
    void publishMqttStatus_();
    void readAndReportSensors_();
    void handleMqttMessage_(const char* topic, float value);
};

} // namespace app
