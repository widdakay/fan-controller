#pragma once
#include "Config.hpp"
#include "app/Types.hpp"
#include "util/Result.hpp"
#include "util/Timer.hpp"
#include <PubSubClient.h>
#include <WiFi.h>
#include <functional>

namespace services {

class MqttClient {
public:
    using MessageCallback = std::function<void(const char* topic, float value)>;

    explicit MqttClient(WiFiClient& wifiClient);

    void begin();
    void setMessageCallback(MessageCallback callback);
    void loop();
    bool publish(const char* topic, const String& payload, bool retained = false);
    bool publishPowerStatus(float powerLevel);

    bool isConnected() {
        return client_.connected();
    }

private:
    void reconnect();
    void messageCallback(char* topic, byte* payload, unsigned int length);

    PubSubClient client_;
    util::Timer reconnectTimer_;
    MessageCallback userCallback_;
};

} // namespace services
