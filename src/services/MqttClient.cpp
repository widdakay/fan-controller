#include "services/MqttClient.hpp"

namespace services {

MqttClient::MqttClient(WiFiClient& wifiClient)
    : client_(wifiClient), reconnectTimer_(config::MQTT_RECONNECT_INTERVAL_MS) {}

void MqttClient::begin() {
    client_.setServer(config::MQTT_SERVER, config::MQTT_PORT);
    client_.setCallback([this](char* topic, byte* payload, unsigned int length) {
        this->messageCallback(topic, payload, length);
    });
}

void MqttClient::setMessageCallback(MessageCallback callback) {
    userCallback_ = callback;
}

void MqttClient::loop() {
    if (!client_.connected()) {
        if (reconnectTimer_.check()) {
            reconnect();
        }
    } else {
        client_.loop();
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
    return publish(config::MQTT_TOPIC_POWER_STATUS, buffer, false);
}

void MqttClient::reconnect() {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    Serial.print("Connecting to MQTT...");

    // Generate client ID from chip ID
    String clientId = "ESP32-";
    clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

    if (client_.connect(clientId.c_str())) {
        Serial.println(" connected");

        // Subscribe to power command topic
        client_.subscribe(config::MQTT_TOPIC_POWER_COMMAND);
        Serial.printf("Subscribed to %s\n", config::MQTT_TOPIC_POWER_COMMAND);
    } else {
        Serial.printf(" failed, rc=%d\n", client_.state());
    }
}

void MqttClient::messageCallback(char* topic, byte* payload, unsigned int length) {
    // Null-terminate payload
    char buffer[length + 1];
    memcpy(buffer, payload, length);
    buffer[length] = '\0';

    Serial.printf("MQTT message: %s = %s\n", topic, buffer);

    // Parse as float
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
