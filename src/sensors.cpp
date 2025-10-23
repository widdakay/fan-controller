#include "sensors.h"

// Helper: convert a 8‐byte DeviceAddress into a 16‐character hex string
String addressToString(const DeviceAddress deviceAddress) {
  String s = "";
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) s += "0";
    s += String(deviceAddress[i], HEX);
  }
  s.toUpperCase();
  return s;
}

void setupSensors() {
  Serial.print("Initializing Dallas Temperature library...");
  sensors.begin();

  // How many sensors are on the bus?
  deviceCount = sensors.getDeviceCount();
  Serial.print("Found ");
  Serial.print(deviceCount);
  Serial.println(" OneWire device(s).");

  // Print out each device's address:
  for (uint8_t i = 0; i < deviceCount; i++) {
    DeviceAddress addr;
    if (sensors.getAddress(addr, i)) {
      Serial.print("  Sensor #");
      Serial.print(i);
      Serial.print(" address: ");
      Serial.println(addressToString(addr));
      // Set resolution for each sensor:
      sensors.setResolution(addr, TEMPERATURE_PRECISION);
    } else {
      Serial.print("  Could not read address for device #");
      Serial.println(i);
    }
  }
}

String getTemperatureData() {
  sensors.requestTemperatures();
  delay(250);

  unsigned long nowMs = millis();
  String data = "{";
  data += "\"measurement\":\"onewire_temp\",";
  data += "\"arduino_millis\":" + String(nowMs) + ",";

  bool firstSensor = true;
  // For each sensor, add its data to the JSON
  for (uint8_t i = 0; i < deviceCount; i++) {
    DeviceAddress addr;
    if (!sensors.getAddress(addr, i)) {
      // If we somehow lost the address, skip this index
      continue;
    }

    float tempC = sensors.getTempC(addr);
    if (tempC == DEVICE_DISCONNECTED_C) {
      Serial.print("Sensor ");
      Serial.print(i);
      Serial.println(" is disconnected!");
      continue;
    }

    String addrTag = addressToString(addr);

    // Add comma between sensors
    if (!firstSensor) {
      data += ",";
    }
    firstSensor = false;

    // Add this sensor's data
    data += "\"" + addrTag + "\":" + String(tempC, 3);

    // Debug print
    Serial.println("-----");
    Serial.print("Sensor #");
    Serial.print(i);
    Serial.print(" (");
    Serial.print(addrTag);
    Serial.print(") → ");
    Serial.print(tempC, 3);
    Serial.println(" °C");
  }

  // Close the JSON structure
  data += "}";
  Serial.println("JSON → " + data);

  return data;
}