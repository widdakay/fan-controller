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
        Logger::info("OTA Update Starting...");
        if (otaCallback_) {
            otaCallback_(true);
        }
    });

    ArduinoOTA.onEnd([this]() {
        Logger::info("OTA Update Complete");
        if (otaCallback_) {
            otaCallback_(false);
        }
    });

    ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
        Logger::info("Progress: %u%%", (progress / (total / 100)));
        // Feed watchdog during OTA to prevent timeout on long updates
        watchdog_.feed();
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Logger::error("OTA Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Logger::error("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Logger::error("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Logger::error("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Logger::error("Receive Failed");
        else if (error == OTA_END_ERROR) Logger::error("End Failed");
    });

    ArduinoOTA.begin();
    Logger::info("Arduino OTA enabled");
}

void OtaManager::setOtaCallback(OtaCallback callback) {
    otaCallback_ = callback;
}

void OtaManager::checkForUpdate() {
    if (!firmwareCheckTimer_.check()) {
        return;
    }

    Logger::info("Checking for firmware update...");

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
            Logger::info("Firmware update available!");
            // TODO: Implement HTTPS OTA download and update
            // This would require downloading the firmware binary
            // and using Update.h to flash it
        } else {
            Logger::info("Firmware is up to date");
        }
    } else {
        Logger::error("Failed to check for firmware update");
    }
}

} // namespace services
