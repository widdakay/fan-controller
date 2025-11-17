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
    using ConfigCallback = std::function<void(const char* topic, const char* payload)>;

    explicit MqttClient(WiFiClient& wifiClient);

    // Initialize with dynamic configuration
    void begin(const String& server, uint16_t port,
               const String& commandTopic, const String& statusTopic);

    void setMessageCallback(MessageCallback callback);
    void setConfigCallback(ConfigCallback callback);
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
    ConfigCallback configCallback_;

    // MQTT configuration
    String mqttServer_;
    uint16_t mqttPort_;
    String topicCommandPower_;
    String topicStatusPower_;
    String topicConfig_;
};

} // namespace services
