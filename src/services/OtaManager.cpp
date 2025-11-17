#include "services/OtaManager.hpp"
#include "util/Logger.hpp"

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
        LOG_INFO("OTA Update Starting...");
        if (otaCallback_) {
            otaCallback_(true);
        }
    });

    ArduinoOTA.onEnd([this]() {
        LOG_INFO("OTA Update Complete");
        if (otaCallback_) {
            otaCallback_(false);
        }
    });

    ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
        LOG_INFO("Progress: %u%%", (progress / (total / 100)));
        // Feed watchdog during OTA to prevent timeout on long updates
        watchdog_.feed();
    });

    ArduinoOTA.onError([](ota_error_t error) {
        LOG_ERROR("OTA Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) LOG_ERROR("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) LOG_ERROR("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) LOG_ERROR("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) LOG_ERROR("Receive Failed");
        else if (error == OTA_END_ERROR) LOG_ERROR("End Failed");
    });

    ArduinoOTA.begin();
    LOG_INFO("Arduino OTA enabled");
}

void OtaManager::setOtaCallback(OtaCallback callback) {
    otaCallback_ = callback;
}

void OtaManager::checkForUpdate() {
    if (!firmwareCheckTimer_.check()) {
        return;
    }

    LOG_INFO("Checking for firmware update...");

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
            LOG_INFO("Firmware update available!");
            // TODO: Implement HTTPS OTA download and update
            // This would require downloading the firmware binary
            // and using Update.h to flash it
        } else {
            LOG_INFO("Firmware is up to date");
        }
    } else {
        LOG_ERROR("Failed to check for firmware update");
    }
}

} // namespace services
