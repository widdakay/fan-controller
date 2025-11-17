#pragma once
#include <Preferences.h>
#include <Arduino.h>
#include <vector>
#include <array>
#include "app/Types.hpp"
#include "util/Result.hpp"
#include "util/Logger.hpp"

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
        : deviceName(config::DEVICE_NAME)
        , mqttServer(config::MQTT_SERVER)
        , mqttPort(config::MQTT_PORT)
        , mqttTopicPowerCommand(config::MQTT_TOPIC_POWER_COMMAND)
        , mqttTopicPowerStatus(config::MQTT_TOPIC_POWER_STATUS)
        , apiInfluxDb(config::API_INFLUXDB)
        , apiFirmwareUpdate(config::API_FW_UPDATE)
    {
        // Add WiFi credentials from SecureConfig
        for (const auto& cred : config::WIFI_CREDENTIALS) {
            wifiCredentials.push_back(WiFiCredential(cred.ssid, cred.password));
        }
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
            Logger::info("[ConfigManager] First boot detected, creating default configuration");
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

        Logger::info("[ConfigManager] Configuration saved to NVS");
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
        Logger::info("========== Device Configuration ==========");
        Logger::info("Device Name: %s", config_.deviceName.c_str());
        Logger::info("WiFi Networks (%d):", config_.wifiCredentials.size());
        for (size_t i = 0; i < config_.wifiCredentials.size(); i++) {
            Logger::info("  %d: %s / %s", i,
                config_.wifiCredentials[i].ssid.c_str(),
                maskPassword(config_.wifiCredentials[i].password).c_str());
        }
        Logger::info("MQTT:");
        Logger::info("  Server: %s:%d", config_.mqttServer.c_str(), config_.mqttPort);
        Logger::info("  Command Topic: %s", config_.mqttTopicPowerCommand.c_str());
        Logger::info("  Status Topic: %s", config_.mqttTopicPowerStatus.c_str());
        Logger::info("API Endpoints:");
        Logger::info("  InfluxDB: %s", config_.apiInfluxDb.c_str());
        Logger::info("  FW Update: %s", config_.apiFirmwareUpdate.c_str());
        Logger::info("==========================================");
    }

private:
    Preferences prefs_;
    DeviceConfig config_;

    /**
     * Load configuration from NVS
     */
    util::Result<void, app::ConfigError> load() {
        // Device info
        config_.deviceName = prefs_.getString("deviceName", config::DEVICE_NAME);

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
        config_.mqttServer = prefs_.getString("mqttServer", config::MQTT_SERVER);
        config_.mqttPort = prefs_.getUShort("mqttPort", config::MQTT_PORT);
        config_.mqttTopicPowerCommand = prefs_.getString("mqttCmdTopic", config::MQTT_TOPIC_POWER_COMMAND);
        config_.mqttTopicPowerStatus = prefs_.getString("mqttStatTopic", config::MQTT_TOPIC_POWER_STATUS);

        // API endpoints
        config_.apiInfluxDb = prefs_.getString("apiInflux", config::API_INFLUXDB);
        config_.apiFirmwareUpdate = prefs_.getString("apiFwUpdate", config::API_FW_UPDATE);

        Logger::info("[ConfigManager] Configuration loaded from NVS");
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
