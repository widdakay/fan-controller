#pragma once
#include <Preferences.h>
#include <Arduino.h>
#include <vector>
#include <array>
#include "app/Types.hpp"
#include "util/Result.hpp"

namespace services {

// WiFi credential structure (dynamic version)
struct WiFiCredential {
    String ssid;
    String password;

    WiFiCredential() = default;
    WiFiCredential(const char* s, const char* p) : ssid(s), password(p) {}
};

// Configuration structure holding all user-configurable values
struct DeviceConfig {
    // Device identification
    String deviceName;

    // WiFi credentials (up to 5 networks)
    std::vector<WiFiCredential> wifiCredentials;

    // MQTT configuration
    String mqttServer;
    uint16_t mqttPort;
    String mqttTopicPowerCommand;
    String mqttTopicPowerStatus;

    // API endpoints
    String apiInfluxDb;
    String apiFirmwareUpdate;

    // Constructor with defaults
    DeviceConfig()
        : deviceName("ESP32-Fan")
        , mqttServer("192.168.1.1")
        , mqttPort(1883)
        , mqttTopicPowerCommand("device/fan/power")
        , mqttTopicPowerStatus("device/fan/power/status")
        , apiInfluxDb("https://data.example.com/particle/log")
        , apiFirmwareUpdate("https://data.example.com/particle/fw/update")
    {
        // Add one default WiFi credential
        wifiCredentials.push_back(WiFiCredential("YourSSID", "YourPassword"));
    }
};

/**
 * ConfigManager - Manages device configuration in ESP32 NVS (flash storage)
 *
 * Responsibilities:
 * - Load configuration from NVS on boot
 * - Save configuration changes to NVS
 * - Provide access to current configuration
 * - Handle first-boot scenario with defaults
 *
 * Usage:
 *   ConfigManager config;
 *   config.begin();
 *   const auto& cfg = config.get();
 *   Serial.println(cfg.deviceName);
 */
class ConfigManager {
public:
    ConfigManager() = default;
    ~ConfigManager() {
        prefs_.end();
    }

    /**
     * Initialize the ConfigManager
     * Loads configuration from NVS, or creates defaults if not found
     *
     * @return Result indicating success or error
     */
    util::Result<void, app::ConfigError> begin() {
        if (!prefs_.begin("device_cfg", false)) {  // false = read-write mode
            return util::Result<void, app::ConfigError>::Err(app::ConfigError::NvsOpenFailed);
        }

        // Check if configuration has been initialized
        bool isInitialized = prefs_.getBool("initialized", false);

        if (!isInitialized) {
            // First boot - save defaults
            Serial.println("[ConfigManager] First boot detected, creating default configuration");
            auto saveResult = saveDefaults();
            if (saveResult.isErr()) {
                return util::Result<void, app::ConfigError>::Err(saveResult.error());
            }
        }

        // Load configuration from NVS
        auto loadResult = load();
        if (loadResult.isErr()) {
            return util::Result<void, app::ConfigError>::Err(loadResult.error());
        }

        return util::Result<void, app::ConfigError>::Ok();
    }

    /**
     * Get read-only access to current configuration
     */
    const DeviceConfig& get() const {
        return config_;
    }

    /**
     * Save current configuration to NVS
     */
    util::Result<void, app::ConfigError> save() {
        prefs_.clear();  // Clear old values

        // Device info
        prefs_.putString("deviceName", config_.deviceName);

        // WiFi credentials
        uint8_t wifiCount = min(config_.wifiCredentials.size(), (size_t)5);
        prefs_.putUChar("wifiCount", wifiCount);

        for (uint8_t i = 0; i < wifiCount; i++) {
            String ssidKey = "wifi" + String(i) + "ssid";
            String passKey = "wifi" + String(i) + "pass";
            prefs_.putString(ssidKey.c_str(), config_.wifiCredentials[i].ssid);
            prefs_.putString(passKey.c_str(), config_.wifiCredentials[i].password);
        }

        // MQTT config
        prefs_.putString("mqttServer", config_.mqttServer);
        prefs_.putUShort("mqttPort", config_.mqttPort);
        prefs_.putString("mqttCmdTopic", config_.mqttTopicPowerCommand);
        prefs_.putString("mqttStatTopic", config_.mqttTopicPowerStatus);

        // API endpoints
        prefs_.putString("apiInflux", config_.apiInfluxDb);
        prefs_.putString("apiFwUpdate", config_.apiFirmwareUpdate);

        // Mark as initialized
        prefs_.putBool("initialized", true);

        Serial.println("[ConfigManager] Configuration saved to NVS");
        return util::Result<void, app::ConfigError>::Ok();
    }

    /**
     * Update device name
     */
    util::Result<void, app::ConfigError> setDeviceName(const String& name) {
        if (name.length() == 0 || name.length() > 32) {
            return util::Result<void, app::ConfigError>::Err(app::ConfigError::InvalidValue);
        }
        config_.deviceName = name;
        return save();
    }

    /**
     * Update MQTT server
     */
    util::Result<void, app::ConfigError> setMqttServer(const String& server, uint16_t port) {
        if (server.length() == 0 || server.length() > 64) {
            return util::Result<void, app::ConfigError>::Err(app::ConfigError::InvalidValue);
        }
        config_.mqttServer = server;
        config_.mqttPort = port;
        return save();
    }

    /**
     * Update MQTT topics
     */
    util::Result<void, app::ConfigError> setMqttTopics(const String& commandTopic, const String& statusTopic) {
        if (commandTopic.length() == 0 || commandTopic.length() > 64 ||
            statusTopic.length() == 0 || statusTopic.length() > 64) {
            return util::Result<void, app::ConfigError>::Err(app::ConfigError::InvalidValue);
        }
        config_.mqttTopicPowerCommand = commandTopic;
        config_.mqttTopicPowerStatus = statusTopic;
        return save();
    }

    /**
     * Add or update WiFi credential
     * @param index Index to add/update (0-4)
     */
    util::Result<void, app::ConfigError> setWifiCredential(uint8_t index, const String& ssid, const String& password) {
        if (index > 4 || ssid.length() == 0 || ssid.length() > 32 ||
            password.length() < 8 || password.length() > 64) {
            return util::Result<void, app::ConfigError>::Err(app::ConfigError::InvalidValue);
        }

        // Expand vector if needed
        while (config_.wifiCredentials.size() <= index) {
            config_.wifiCredentials.push_back(WiFiCredential("", ""));
        }

        config_.wifiCredentials[index].ssid = ssid;
        config_.wifiCredentials[index].password = password;

        return save();
    }

    /**
     * Update API endpoints
     */
    util::Result<void, app::ConfigError> setApiEndpoints(const String& influxDb, const String& fwUpdate) {
        if (influxDb.length() == 0 || influxDb.length() > 128 ||
            fwUpdate.length() == 0 || fwUpdate.length() > 128) {
            return util::Result<void, app::ConfigError>::Err(app::ConfigError::InvalidValue);
        }
        config_.apiInfluxDb = influxDb;
        config_.apiFirmwareUpdate = fwUpdate;
        return save();
    }

    /**
     * Reset to factory defaults
     */
    util::Result<void, app::ConfigError> resetToDefaults() {
        config_ = DeviceConfig();  // Reset to default constructor values
        return save();
    }

    /**
     * Print current configuration to Serial
     */
    void printConfig() const {
        Serial.println("\n========== Device Configuration ==========");
        Serial.printf("Device Name: %s\n", config_.deviceName.c_str());
        Serial.printf("\nWiFi Networks (%d):\n", config_.wifiCredentials.size());
        for (size_t i = 0; i < config_.wifiCredentials.size(); i++) {
            Serial.printf("  %d: %s / %s\n", i,
                config_.wifiCredentials[i].ssid.c_str(),
                maskPassword(config_.wifiCredentials[i].password).c_str());
        }
        Serial.printf("\nMQTT:\n");
        Serial.printf("  Server: %s:%d\n", config_.mqttServer.c_str(), config_.mqttPort);
        Serial.printf("  Command Topic: %s\n", config_.mqttTopicPowerCommand.c_str());
        Serial.printf("  Status Topic: %s\n", config_.mqttTopicPowerStatus.c_str());
        Serial.printf("\nAPI Endpoints:\n");
        Serial.printf("  InfluxDB: %s\n", config_.apiInfluxDb.c_str());
        Serial.printf("  FW Update: %s\n", config_.apiFirmwareUpdate.c_str());
        Serial.println("==========================================\n");
    }

private:
    Preferences prefs_;
    DeviceConfig config_;

    /**
     * Load configuration from NVS
     */
    util::Result<void, app::ConfigError> load() {
        // Device info
        config_.deviceName = prefs_.getString("deviceName", "ESP32-Fan");

        // WiFi credentials
        uint8_t wifiCount = prefs_.getUChar("wifiCount", 0);
        config_.wifiCredentials.clear();

        for (uint8_t i = 0; i < wifiCount && i < 5; i++) {
            String ssidKey = "wifi" + String(i) + "ssid";
            String passKey = "wifi" + String(i) + "pass";
            String ssid = prefs_.getString(ssidKey.c_str(), "");
            String pass = prefs_.getString(passKey.c_str(), "");

            if (ssid.length() > 0) {
                config_.wifiCredentials.push_back(WiFiCredential(ssid.c_str(), pass.c_str()));
            }
        }

        // MQTT config
        config_.mqttServer = prefs_.getString("mqttServer", "192.168.1.1");
        config_.mqttPort = prefs_.getUShort("mqttPort", 1883);
        config_.mqttTopicPowerCommand = prefs_.getString("mqttCmdTopic", "device/fan/power");
        config_.mqttTopicPowerStatus = prefs_.getString("mqttStatTopic", "device/fan/power/status");

        // API endpoints
        config_.apiInfluxDb = prefs_.getString("apiInflux", "https://data.example.com/particle/log");
        config_.apiFirmwareUpdate = prefs_.getString("apiFwUpdate", "https://data.example.com/particle/fw/update");

        Serial.println("[ConfigManager] Configuration loaded from NVS");
        return util::Result<void, app::ConfigError>::Ok();
    }

    /**
     * Save default configuration to NVS
     */
    util::Result<void, app::ConfigError> saveDefaults() {
        config_ = DeviceConfig();  // Use default constructor
        return save();
    }

    /**
     * Mask password for display
     */
    String maskPassword(const String& password) const {
        if (password.length() <= 4) {
            return "****";
        }
        return password.substring(0, 2) + "****" + password.substring(password.length() - 2);
    }
};

} // namespace services
