#include "services/OtaManager.hpp"

namespace services {

OtaManager::OtaManager(HttpsClient& httpsClient, WatchdogService& watchdog)
    : httpsClient_(httpsClient),
      watchdog_(watchdog),
      firmwareCheckTimer_(config::TASK_FW_CHECK_MS) {}

void OtaManager::begin(const String& deviceName, const String& fwUpdateUrl) {
    deviceName_ = deviceName;
    fwUpdateUrl_ = fwUpdateUrl;

    // Setup Arduino OTA
    ArduinoOTA.setHostname(deviceName_.c_str());

    ArduinoOTA.onStart([this]() {
        Serial.println("OTA Update Starting...");
        if (otaCallback_) {
            otaCallback_(true);
        }
    });

    ArduinoOTA.onEnd([this]() {
        Serial.println("\nOTA Update Complete");
        if (otaCallback_) {
            otaCallback_(false);
        }
    });

    ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
        // Feed watchdog during OTA to prevent timeout on long updates
        watchdog_.feed();
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("OTA Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

    ArduinoOTA.begin();
    Serial.println("Arduino OTA enabled");
}

void OtaManager::setOtaCallback(OtaCallback callback) {
    otaCallback_ = callback;
}

void OtaManager::checkForUpdate() {
    if (!firmwareCheckTimer_.check()) {
        return;
    }

    Serial.println("Checking for firmware update...");

    // Get chip ID
    uint64_t chipId = ESP.getEfuseMac();
    String currentVersion = FIRMWARE_VERSION;

    // Build JSON request
    StaticJsonDocument<200> doc;
    doc["ID"] = String((uint32_t)chipId, HEX);
    doc["ver"] = currentVersion;

    String jsonRequest;
    serializeJson(doc, jsonRequest);

    // Check if update available
    auto response = httpsClient_.post(fwUpdateUrl_.c_str(), jsonRequest);

    if (response.isOk()) {
        String responseStr = response.value();
        responseStr.trim();

        if (responseStr == "True" || responseStr == "true") {
            Serial.println("Firmware update available!");
            // TODO: Implement HTTPS OTA download and update
            // This would require downloading the firmware binary
            // and using Update.h to flash it
        } else {
            Serial.println("Firmware is up to date");
        }
    } else {
        Serial.println("Failed to check for firmware update");
    }
}

} // namespace services
