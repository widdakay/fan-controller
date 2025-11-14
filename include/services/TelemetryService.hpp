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

        // Temperatures - use serialized() to force float notation
        char tempStr[16];
        if (health.motorTemp.inRange) {
            snprintf(tempStr, sizeof(tempStr), "%.6f", health.motorTemp.tempC);
            fields["motor_temp_c"] = serialized(tempStr);
        }
        if (health.mcuExternalTemp.inRange) {
            snprintf(tempStr, sizeof(tempStr), "%.6f", health.mcuExternalTemp.tempC);
            fields["mcu_external_temp_c"] = serialized(tempStr);
        }
        if (std::isfinite(health.mcuInternalTempC)) {
            snprintf(tempStr, sizeof(tempStr), "%.6f", health.mcuInternalTempC);
            fields["mcu_internal_temp_c"] = serialized(tempStr);
        }

        // Power rails - use serialized() to force float notation
        if (std::isfinite(health.rail3v3)) {
            snprintf(tempStr, sizeof(tempStr), "%.6f", health.rail3v3);
            fields["rail_3v3"] = serialized(tempStr);
        }
        if (std::isfinite(health.rail5v)) {
            snprintf(tempStr, sizeof(tempStr), "%.6f", health.rail5v);
            fields["rail_5v"] = serialized(tempStr);
        }

        // Input power - use serialized() to force float notation
        if (health.inputPower.valid) {
            snprintf(tempStr, sizeof(tempStr), "%.6f", health.inputPower.busVolts);
            fields["v_in"] = serialized(tempStr);
            snprintf(tempStr, sizeof(tempStr), "%.6f", health.inputPower.currentMilliamps / 1000.0f);
            fields["i_in"] = serialized(tempStr);
            snprintf(tempStr, sizeof(tempStr), "%.6f", health.inputPower.shuntMillivolts);
            fields["v_shunt"] = serialized(tempStr);
            snprintf(tempStr, sizeof(tempStr), "%.6f", health.inputPower.powerMilliwatts / 1000.0f);
            fields["p_in"] = serialized(tempStr);
        }

        // Motor status - use serialized() with formatted string to force float type
        // This prevents ArduinoJson from optimizing 0.0 to integer 0
        char dutyStr[16];
        snprintf(dutyStr, sizeof(dutyStr), "%.6f", health.motor.dutyCycle);
        fields["motor_duty"] = serialized(dutyStr);
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
                       const JsonObject& fields, uint64_t serialNum = 0, const String& sensorName = String()) {
        uint32_t timestamp = millis();
        Serial.printf("[%u] sendSensorData: measurement=%s, busId=%u, sensorName=%s, doc capacity=%zu, doc usage=%zu\n",
                     timestamp, measurement, busId, sensorName.isEmpty() ? "null" : sensorName.c_str(), batchDoc_.capacity(), batchDoc_.memoryUsage());
        

        
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

        if (!sensorName.isEmpty()) {
            tags["sensor_name"] = sensorName;  // String object will be copied by ArduinoJson
            Serial.printf("[%lu] sendSensorData: set sensor_name tag to '%s'\n", millis(), sensorName.c_str());
        }

        // Copy fields - need to copy from source JsonObject to new one
        // IMPORTANT: Must copy both keys and values as source document may be destroyed
        JsonObject docFields = doc.createNestedObject("fields");

        size_t fieldCount = 0;
        for (JsonPair kv : fields) {
            // CRITICAL: Make a copy of the key string to prevent dangling pointer
            // when source document goes out of scope
            String keyCopy = kv.key().c_str();
            JsonVariantConst value = kv.value();

            // Copy by value type - use serialized() for all numeric types to force
            // decimal notation and prevent InfluxDB type conflicts
            // Exception: Some fields should remain as integers
            bool keepAsInteger = (keyCopy == "gas_resistance" || keyCopy == "resistance");

            char numStr[32];
            if (value.is<float>() || value.is<double>()) {
                if (keepAsInteger) {
                    // Keep as integer (no decimal point)
                    snprintf(numStr, sizeof(numStr), "%d", (int)value.as<float>());
                    docFields[keyCopy] = serialized(numStr);
                } else {
                    // Float/double - format with decimals
                    snprintf(numStr, sizeof(numStr), "%.6f", value.as<float>());
                    docFields[keyCopy] = serialized(numStr);
                }
            } else if (value.is<int>()) {
                if (keepAsInteger) {
                    // Keep as integer
                    docFields[keyCopy] = value.as<int>();
                } else {
                    // Integer - format as float to maintain type consistency
                    snprintf(numStr, sizeof(numStr), "%d.0", value.as<int>());
                    docFields[keyCopy] = serialized(numStr);
                }
            } else if (value.is<unsigned int>()) {
                if (keepAsInteger) {
                    // Keep as integer
                    docFields[keyCopy] = value.as<unsigned int>();
                } else {
                    snprintf(numStr, sizeof(numStr), "%u.0", value.as<unsigned int>());
                    docFields[keyCopy] = serialized(numStr);
                }
            } else if (value.is<long>()) {
                if (keepAsInteger) {
                    // Keep as integer
                    docFields[keyCopy] = value.as<long>();
                } else {
                    snprintf(numStr, sizeof(numStr), "%ld.0", value.as<long>());
                    docFields[keyCopy] = serialized(numStr);
                }
            } else if (value.is<unsigned long>()) {
                if (keepAsInteger) {
                    // Keep as integer
                    docFields[keyCopy] = value.as<unsigned long>();
                } else {
                    snprintf(numStr, sizeof(numStr), "%lu.0", value.as<unsigned long>());
                    docFields[keyCopy] = serialized(numStr);
                }
            } else if (value.is<bool>()) {
                // Booleans as integers (0 or 1)
                docFields[keyCopy] = value.as<bool>() ? 1 : 0;
            } else if (value.is<const char*>()) {
                // String values also need to be copied
                docFields[keyCopy] = String(value.as<const char*>());
            } else {
                // Fallback: try direct assignment
                docFields[keyCopy] = value;
            }
            fieldCount++;
            Serial.printf("[%lu] sendSensorData: copied field %s\n", millis(), keyCopy.c_str());
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
            char tempStr[16];
            snprintf(tempStr, sizeof(tempStr), "%.6f", reading.tempC);
            fields["temp_c"] = serialized(tempStr);
        }
    }

    void sendBootInfo(const app::BootInfo& boot, const std::vector<app::WiFiScanResult>& wifiScan) {
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
        // Force floating point precision to ensure 0.0 doesn't serialize as 0
        serializeJson(batchArray_, jsonData);

        Serial.printf("[%lu] flushBatch: serialized %zu bytes, %zu points\n", millis(), jsonData.length(), batchSize);
        Serial.printf("[%lu] flushBatch: JSON data: %s\n", millis(), jsonData.c_str());

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
