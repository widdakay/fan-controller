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
        : httpsClient_(httpsClient) {
        batchArray_ = batchDoc_.to<JsonArray>();
    }

    void sendHealthReport(const app::HealthData& health) {
        JsonObject doc = batchArray_.createNestedObject();

        doc["measurement"] = "ESP_Health";

        // Tags
        JsonObject tags = doc.createNestedObject("tags");
        tags["device"] = config::DEVICE_NAME;
        tags["chip_id"] = String((uint64_t)ESP.getEfuseMac(), HEX);

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
    }

    void sendSensorData(const char* measurement, uint8_t busId,
                       const JsonObject& fields, uint64_t serialNum = 0) {
        uint32_t timestamp = millis();
        Serial.printf("[%u] sendSensorData: measurement=%s, busId=%u, doc capacity=%zu, doc usage=%zu\n", 
                     timestamp, measurement, busId, batchDoc_.capacity(), batchDoc_.memoryUsage());
        

        
        // Check array size before creating object
        size_t arraySizeBefore = batchArray_.size();
        JsonObject doc = batchArray_.createNestedObject();
        size_t arraySizeAfter = batchArray_.size();
        
        // Verify the object was created (array size should increase)
        if (arraySizeAfter == arraySizeBefore) {
            // Object creation failed - document is likely full
            Serial.printf("[%lu] sendSensorData: ERROR - failed to create nested object (doc full?), flushing and retrying\n", millis());
            flushBatch();
            batchArray_ = batchDoc_.to<JsonArray>();
            doc = batchArray_.createNestedObject();
            
            // Check again
            if (batchArray_.size() == 0) {
                Serial.printf("[%lu] sendSensorData: ERROR - still failed after flush, skipping this measurement\n", millis());
                return;
            }
        }
        
        doc["measurement"] = measurement;

        // Tags
        JsonObject tags = doc.createNestedObject("tags");
        tags["device"] = config::DEVICE_NAME;
        tags["chip_id"] = String((uint64_t)ESP.getEfuseMac(), HEX);
        tags["bus_id"] = busId;

        if (serialNum != 0) {
            tags["serial"] = String((uint64_t)serialNum, HEX);
        }

        // Copy fields - need to copy from source JsonObject to new one
        // IMPORTANT: Copy values immediately while source document is still in scope
        JsonObject docFields = doc.createNestedObject("fields");
        
        size_t fieldCount = 0;
        for (JsonPair kv : fields) {
            const char* key = kv.key().c_str();
            JsonVariantConst value = kv.value();
            
            // Copy by value type to ensure we get actual values, not references
            if (value.is<float>()) {
                docFields[key] = value.as<float>();
            } else if (value.is<int>()) {
                docFields[key] = value.as<int>();
            } else if (value.is<unsigned int>()) {
                docFields[key] = value.as<unsigned int>();
            } else if (value.is<long>()) {
                docFields[key] = value.as<long>();
            } else if (value.is<unsigned long>()) {
                docFields[key] = value.as<unsigned long>();
            } else if (value.is<double>()) {
                docFields[key] = value.as<double>();
            } else {
                // Fallback: try direct assignment
                docFields[key] = value;
            }
            fieldCount++;
            Serial.printf("[%lu] sendSensorData: copied field %s\n", millis(), key);
        }
        
        Serial.printf("[%lu] sendSensorData: copied %zu fields from source\n", millis(), fieldCount);
        
        // Always add timestamp - ensures at least one field (InfluxDB requirement)
        docFields["arduino_millis"] = timestamp;
        
        // Verify fields were actually set by checking if they exist
        size_t verifiedCount = docFields.size();
        Serial.printf("[%lu] sendSensorData: verified %zu fields in docFields, doc usage=%zu\n", 
                     millis(), verifiedCount, batchDoc_.memoryUsage());
    }

    void sendOneWireData(const std::vector<app::OneWireReading>& readings) {
        uint32_t timestamp = millis();
        for (const auto& reading : readings) {
            if (!reading.valid) continue;

            JsonObject doc = batchArray_.createNestedObject();
            doc["measurement"] = "onewire_temp";

            JsonObject tags = doc.createNestedObject("tags");
            tags["device"] = config::DEVICE_NAME;
            tags["chip_id"] = String((uint64_t)ESP.getEfuseMac(), HEX);
            tags["bus_id"] = reading.busId;
            tags["address"] = String((uint32_t)reading.address, HEX);

            JsonObject fields = doc.createNestedObject("fields");
            fields["arduino_millis"] = timestamp;
            fields["temp_c"] = reading.tempC;
        }
    }

    void sendBootInfo(const app::BootInfo& boot, const std::vector<app::WiFiScanResult>& wifiScan, const app::HardwareConfig& hwConfig) {
        JsonObject doc = batchArray_.createNestedObject();

        doc["measurement"] = "ESP_Boot";

        JsonObject tags = doc.createNestedObject("tags");
        tags["device"] = config::DEVICE_NAME;
        tags["chip_id"] = String((uint64_t)boot.chipId, HEX);

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

        // Add hardware configuration as JSON tree
        JsonObject hwConfigJson = fields.createNestedObject("hardware_config");

        // ESP32 root node
        JsonObject esp32 = hwConfigJson.createNestedObject("esp32");
        esp32["chip_id"] = String((uint64_t)hwConfig.chipId, HEX);
        esp32["firmware_version"] = hwConfig.firmwareVersion;

        // Onboard hardware
        JsonObject onboard = esp32.createNestedObject("onboard");
        onboard["ads1115"] = hwConfig.ads1115Initialized;
        onboard["ina226"] = hwConfig.ina226Initialized;
        onboard["motor_controller"] = hwConfig.motorControllerInitialized;

        // I2C buses
        JsonObject i2cBuses = esp32.createNestedObject("i2c_buses");
        for (const auto& bus : hwConfig.i2cBuses) {
            JsonObject busJson = i2cBuses.createNestedObject(String(bus.busId));
            busJson["sda_pin"] = bus.sdaPin;
            busJson["scl_pin"] = bus.sclPin;

            JsonObject sensors = busJson.createNestedObject("sensors");
            for (const auto& sensor : bus.sensors) {
                JsonObject sensorJson = sensors.createNestedObject(String(sensor.address, HEX));
                sensorJson["type"] = sensor.type;
                sensorJson["initialized"] = sensor.initialized;
            }
        }

        // OneWire buses
        JsonObject oneWireBuses = esp32.createNestedObject("onewire_buses");
        for (const auto& bus : hwConfig.oneWireBuses) {
            JsonObject busJson = oneWireBuses.createNestedObject(String(bus.busId));
            busJson["pin"] = bus.pin;
            busJson["device_count"] = bus.deviceCount;

            JsonArray sensors = busJson.createNestedArray("sensors");
            for (const auto& sensor : bus.sensors) {
                JsonObject sensorJson = sensors.createNestedObject();
                sensorJson["address"] = String((uint64_t)sensor.address, HEX);
                sensorJson["valid"] = sensor.valid;
            }
        }
    }

    void flushBatch() {
        uint32_t timestamp = millis();
        size_t arraySize = batchArray_.size();
        size_t docUsage = batchDoc_.memoryUsage();
        Serial.printf("[%u] flushBatch: batchArray size=%zu, doc usage=%zu/%zu\n", 
                     timestamp, arraySize, docUsage, batchDoc_.capacity());
        
        if (arraySize == 0) {
            // Even if array is empty, if document is nearly full, we should clear it
            // to free memory for future operations
            if (docUsage > batchDoc_.capacity() * 0.8) {
                Serial.printf("[%lu] flushBatch: array empty but doc nearly full, clearing document\n", millis());
                batchDoc_.clear();
                batchArray_ = batchDoc_.to<JsonArray>();
            } else {
                Serial.printf("[%lu] flushBatch: nothing to send, returning\n", millis());
            }
            return;  // Nothing to send
        }

        size_t batchSize = arraySize;
        String jsonData;
        serializeJson(batchArray_, jsonData);

        Serial.printf("[%lu] flushBatch: serialized %zu bytes, %zu points\n", millis(), jsonData.length(), batchSize);

        // Clear the batch for next use before sending (in case send fails)
        batchArray_.clear();
        // Also clear the document to free memory (StaticJsonDocument doesn't auto-free on array clear)
        batchDoc_.clear();
        batchArray_ = batchDoc_.to<JsonArray>();

        sendData(jsonData, batchSize);
    }

private:
    void sendData(const String& jsonData, size_t batchSize) {
        uint32_t timestamp = millis();
        Serial.printf("[%u] sendData: sending batch of %zu points\n", timestamp, batchSize);
        
        auto result = httpsClient_.post(config::API_INFLUXDB, jsonData);

        if (result.isOk()) {
            Serial.printf("[%lu] Telemetry batch sent successfully (%zu points)\n", millis(), batchSize);
        } else {
            Serial.printf("[%lu] Failed to send telemetry batch\n", millis());
            // Could flash error LED here
        }
    }

    HttpsClient& httpsClient_;
    StaticJsonDocument<8192> batchDoc_;
    JsonArray batchArray_;
};

} // namespace services
