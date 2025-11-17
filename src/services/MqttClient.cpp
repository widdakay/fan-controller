#include "services/MqttClient.hpp"
#include "util/Logger.hpp"

namespace services {

MqttClient::MqttClient(WiFiClient& wifiClient)
    : client_(wifiClient), reconnectTimer_(config::MQTT_RECONNECT_INTERVAL_MS) {}

void MqttClient::begin(const String& server, uint16_t port,
                       const String& commandTopic, const String& statusTopic) {
    mqttServer_ = server;
    mqttPort_ = port;
    topicCommandPower_ = commandTopic;
    topicStatusPower_ = statusTopic;
    topicConfig_ = commandTopic.substring(0, commandTopic.lastIndexOf('/')) + "/config";

    client_.setServer(mqttServer_.c_str(), mqttPort_);
    client_.setKeepAlive(60);  // 60 second keepalive
    client_.setCallback([this](char* topic, byte* payload, unsigned int length) {
        this->messageCallback(topic, payload, length);
    });
}

void MqttClient::setMessageCallback(MessageCallback callback) {
    userCallback_ = callback;
}

void MqttClient::setConfigCallback(ConfigCallback callback) {
    configCallback_ = callback;
}

void MqttClient::loop() {
    // Always call client.loop() to handle MQTT protocol and detect disconnections
    client_.loop();

    // Check if we need to reconnect
    if (!client_.connected()) {
        if (reconnectTimer_.check()) {
            LOG_WARN("MQTT disconnected, attempting reconnect...");
            reconnect();
        }
    }
}

bool MqttClient::publish(const char* topic, const String& payload, bool retained) {
    if (!client_.connected()) {
        return false;
    }
    return client_.publish(topic, payload.c_str(), retained);
}

bool MqttClient::publishPowerStatus(float powerLevel) {
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%.3f", powerLevel);
    return publish(topicStatusPower_.c_str(), buffer, false);
}

void MqttClient::reconnect() {
    if (WiFi.status() != WL_CONNECTED) {
        LOG_DEBUG("WiFi not connected, skipping MQTT reconnect");
        return;
    }

    LOG_INFO("Connecting to MQTT...");

    // Generate client ID from chip ID
    String clientId = "ESP32-";
    clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

    if (client_.connect(clientId.c_str())) {
        LOG_INFO("MQTT connected successfully");

        // Subscribe to power command topic
        client_.subscribe(topicCommandPower_.c_str());
        LOG_INFO("Subscribed to %s", topicCommandPower_.c_str());

        // Subscribe to config topic
        client_.subscribe(topicConfig_.c_str());
        LOG_INFO("Subscribed to %s", topicConfig_.c_str());
    } else {
        LOG_ERROR("MQTT connection failed, rc=%d", client_.state());
    }
}

void MqttClient::messageCallback(char* topic, byte* payload, unsigned int length) {
    // Null-terminate payload
    char buffer[length + 1];
    memcpy(buffer, payload, length);
    buffer[length] = '\0';

    LOG_INFO("MQTT message received: %s = %s", topic, buffer);

    // Check if this is a config message
    if (strcmp(topic, topicConfig_.c_str()) == 0) {
        if (configCallback_) {
            configCallback_(topic, buffer);
        }
        return;
    }

    // Otherwise, parse as float for power command
    float value = atof(buffer);

    // Clamp to 0.0-1.0
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;

    // Call user callback
    if (userCallback_) {
        userCallback_(topic, value);
    }
}

} // namespace services
