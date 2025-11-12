#pragma once
#include "Config.hpp"
#include "HttpsClient.hpp"
#include "util/Timer.hpp"
#include <ArduinoOTA.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <functional>

namespace services {

class OtaManager {
public:
    using OtaCallback = std::function<void(bool active)>;

    explicit OtaManager(HttpsClient& httpsClient);

    void begin();
    void setOtaCallback(OtaCallback callback);

    void handle() {
        ArduinoOTA.handle();
    }

    void checkForUpdate();

private:
    HttpsClient& httpsClient_;
    util::Timer firmwareCheckTimer_;
    OtaCallback otaCallback_;
};

} // namespace services
