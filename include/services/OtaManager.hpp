#pragma once
#include "Config.hpp"
#include "HttpsClient.hpp"
#include "WatchdogService.hpp"
#include "util/Timer.hpp"
#include <ArduinoOTA.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <functional>

namespace services {

class OtaManager {
public:
    using OtaCallback = std::function<void(bool active)>;

    explicit OtaManager(HttpsClient& httpsClient, WatchdogService& watchdog);

    void begin(const String& deviceName, const String& fwUpdateUrl);
    void setOtaCallback(OtaCallback callback);

    void handle() {
        ArduinoOTA.handle();
    }

    void checkForUpdate();

private:
    HttpsClient& httpsClient_;
    WatchdogService& watchdog_;
    util::Timer firmwareCheckTimer_;
    OtaCallback otaCallback_;
    String deviceName_;
    String fwUpdateUrl_;
};

} // namespace services
