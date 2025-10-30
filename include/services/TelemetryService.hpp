#pragma once
#include "Config.hpp"
#include "HttpsClient.hpp"
#include "app/Types.hpp"
#include "util/RingBuffer.hpp"
#include <ArduinoJson.h>
#include <ESP.h>

namespace services {

class TelemetryService {
public:
    explicit TelemetryService(HttpsClient& httpsClient)
        : httpsClient_(httpsClient) {}

    void sendHealthReport(const app::HealthData& health) {
        StaticJsonDocument<1024> doc;

        doc["measurement"] = "ESP_Health";

        // Tags
        JsonObject tags = doc.createNestedObject("tags");
        tags["device"] = config::DEVICE_NAME;
        tags["chip_id"] = String((uint32_t)ESP.getEfuseMac(), HEX);

        // Fields
        JsonObject fields = doc.createNestedObject("fields");
        fields["arduino_millis"] = health.uptimeMs;

        // Temperatures
        if (health.motorTemp.inRange) {
            fields["motor_temp_c"] = health.motorTemp.tempC;
        }
        if (health.mcuExternalTemp.inRange) {
            fields["mcu_external_temp_c"] = health.mcuExternalTemp.tempC;
        }
        if (std::isfinite(health.mcuInternalTempC)) {
            fields["mcu_internal_temp_c"] = health.mcuInternalTempC;
        }

        // Power rails
        if (std::isfinite(health.rail3v3)) {
            fields["rail_3v3"] = health.rail3v3;
        }
        if (std::isfinite(health.rail5v)) {
            fields["rail_5v"] = health.rail5v;
        }

        // Input power
        if (health.inputPower.valid) {
            fields["v_in"] = health.inputPower.busVolts;
            fields["i_in"] = health.inputPower.currentMilliamps / 1000.0f;
            fields["v_shunt"] = health.inputPower.shuntMillivolts;
            fields["p_in"] = health.inputPower.powerMilliwatts / 1000.0f;
        }

        // Motor status
        fields["motor_duty"] = float(health.motor.dutyCycle);  // print 0 as 0.0 not 0
        fields["motor_direction"] = health.motor.directionForward ? 1 : 0;
        fields["motor_en_a"] = health.motor.enAEnabled ? 1 : 0;
        fields["motor_en_b"] = health.motor.enBEnabled ? 1 : 0;
        fields["motor_fault"] = health.motor.fault ? 1 : 0;

        // System info
        fields["free_heap"] = health.freeHeap;
        fields["wifi_rssi"] = health.wifiRssi;
        fields["mqtt_connected"] = health.mqttConnected ? 1 : 0;

        String jsonData;
        serializeJson(doc, jsonData);

        sendData(jsonData);
    }

    void sendSensorData(const char* measurement, uint8_t busId,
                       const JsonObject& fields, uint64_t serialNum = 0) {
        StaticJsonDocument<512> doc;

        doc["measurement"] = measurement;

        // Tags
        JsonObject tags = doc.createNestedObject("tags");
        tags["device"] = config::DEVICE_NAME;
        tags["chip_id"] = String((uint32_t)ESP.getEfuseMac(), HEX);
        tags["bus_id"] = busId;

        if (serialNum != 0) {
            tags["serial"] = String((uint32_t)serialNum, HEX);
        }

        // Copy fields
        doc["fields"] = fields;
        doc["fields"]["arduino_millis"] = millis();

        String jsonData;
        serializeJson(doc, jsonData);

        sendData(jsonData);
    }

    void sendOneWireData(const std::vector<app::OneWireReading>& readings) {
        for (const auto& reading : readings) {
            if (!reading.valid) continue;

            StaticJsonDocument<512> doc;
            doc["measurement"] = "onewire_temp";

            JsonObject tags = doc.createNestedObject("tags");
            tags["device"] = config::DEVICE_NAME;
            tags["chip_id"] = String((uint32_t)ESP.getEfuseMac(), HEX);
            tags["bus_id"] = reading.busId;
            tags["address"] = String((uint32_t)reading.address, HEX);

            JsonObject fields = doc.createNestedObject("fields");
            fields["arduino_millis"] = millis();
            fields["temp_c"] = reading.tempC;

            String jsonData;
            serializeJson(doc, jsonData);

            sendData(jsonData);
        }
    }

    void sendBootInfo(const app::BootInfo& boot, const std::vector<app::WiFiScanResult>& wifiScan) {
        StaticJsonDocument<1024> doc;

        doc["measurement"] = "ESP_Boot";

        JsonObject tags = doc.createNestedObject("tags");
        tags["device"] = config::DEVICE_NAME;
        tags["chip_id"] = String((uint32_t)boot.chipId, HEX);

        JsonObject fields = doc.createNestedObject("fields");
        fields["reset_reason"] = boot.resetReason;
        fields["sketch_size"] = boot.sketchSize;
        fields["free_sketch_space"] = boot.freeSketchSpace;
        fields["heap_size"] = boot.heapSize;
        fields["firmware_version"] = boot.firmwareVersion;
        fields["wifi_networks_found"] = wifiScan.size();

        // Add WiFi scan results
        String wifiList = "";
        for (const auto& wifi : wifiScan) {
            wifiList += wifi.ssid.c_str();
            wifiList += "(";
            wifiList += String(wifi.rssi);
            wifiList += "),";
        }
        fields["wifi_list"] = wifiList.c_str();

        String jsonData;
        serializeJson(doc, jsonData);

        sendData(jsonData);
    }

private:
    void sendData(const String& jsonData) {
        auto result = httpsClient_.post(config::API_INFLUXDB, jsonData);

        if (result.isOk()) {
            Serial.println("Telemetry sent successfully");
        } else {
            Serial.println("Failed to send telemetry");
            // Could flash error LED here
        }
    }

    HttpsClient& httpsClient_;
};

} // namespace services
