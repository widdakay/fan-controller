#include "mqtt.h"
#include "pwm.h"

//-----------------------------------------------------------------------------
// MQTT callback: invoked when a new message arrives on any subscribed topic
//-----------------------------------------------------------------------------
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("New MQTT Message");
  // Only handle `power_topic` (e.g. "fan/power")
  if (strcmp(topic, power_topic) == 0) {
    // Copy payload into a null-terminated buffer
    char msgBuf[32] = { 0 };
    if (length < sizeof(msgBuf)) {
      memcpy(msgBuf, payload, length);
      msgBuf[length] = '\0';
      // Convert ASCII to float
      float newVal = atof(msgBuf);
      // Clamp between 0.0 and 1.0
      if (newVal < 0.0f) newVal = 0.0f;
      if (newVal > 1.0f) newVal = 1.0f;

      // If changed, update and re-compute PWM
      if (fabs(newVal - power) > 0.0001f) {
        power = newVal;
        Serial.printf("MQTT → new power = %.3f\n", power);
        set_vals();
      }
    }
  }
}

//-----------------------------------------------------------------------------
// Ensure we are connected to the broker, and (re-)subscribe if needed
//-----------------------------------------------------------------------------
void reconnectMqtt() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT broker… ");
    if (mqttClient.connect("ESP32FanController")) {
      Serial.println("connected.");
      // Once connected, subscribe to our topic
      mqttClient.subscribe(power_topic);
      Serial.printf("Subscribed to topic \"%s\"\n", power_topic);
    } else {
      Serial.print("failed (rc=");
      Serial.print(mqttClient.state());
      Serial.println("), retrying in 2s");
      delay(2000);
    }
  }
}