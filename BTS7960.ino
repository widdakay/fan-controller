#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <driver/ledc.h>  // ESP-IDF LEDC driver
#include <OneWire.h>
#include <DallasTemperature.h>

// —————— USER CONFIGURATION ——————
#define ONE_WIRE_BUS 23
#define TEMPERATURE_PRECISION 12
static const int EN_A = 23;
static const int EN_B = 22;
static const int PWM_A = 19;
static const int PWM_B = 18;
static const int ISNS_A = 35;
static const int ISNS_B = 34;

// LEDC configuration
static const ledc_timer_t LEDC_TIMER = LEDC_TIMER_0;
static const ledc_mode_t LEDC_MODE = LEDC_HIGH_SPEED_MODE;
static const ledc_timer_bit_t LEDC_RESOLUTION = LEDC_TIMER_8_BIT;  // 8-bit (0…255)
static const int LEDC_FREQUENCY = 20000;                           // 20 kHz

// We'll drive PWM_A on LEDC channel 0, PWM_B on channel 1:
static const ledc_channel_t LEDC_CHANNEL_A = LEDC_CHANNEL_0;
static const ledc_channel_t LEDC_CHANNEL_B = LEDC_CHANNEL_1;

// Dead-time and static-friction math
#define PWM_MAX 256
float dead_time_us = 6.8;  // µs of dead-time
float drive_volts = 12.0;
float static_friction_volts = 0.5;

// “power” ∈ [0.0 … 1.0], set via MQTT callback
volatile float power = 0.0;

int pwm_a = 0;
int pwm_b = 0;

float us_period;
float dead_ticks;

//— MQTT/Wi-Fi configuration —------------------------------------------------

const char* ssid = "CasaIOT";
const char* password = "LetMeIOT";

const char* mqtt_server = "10.10.1.20";  // <— your broker’s IP or hostname
const int mqtt_port = 1883;

// Change this to whatever topic you want to listen on:
const char* power_topic = "home/fan1/power";

WiFiClient espClient;
PubSubClient mqttClient(espClient);


const char* ca_cert =
  "-----BEGIN CERTIFICATE-----\n"
  "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\n"
  "-----END CERTIFICATE-----\n";

const char server[] = "data.yoerik.com";  // your HTTPS endpoint
WiFiClientSecure client;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

uint8_t deviceCount = 0;

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

// Forward declarations
void reconnectMqtt();
void callback(char* topic, byte* payload, unsigned int length);

//-----------------------------------------------------------------------------
// Configure LEDC Timer 0 for 20 kHz, 8-bit resolution
//-----------------------------------------------------------------------------
void setupLEDC() {
  // 1) LEDC timer config:
  ledc_timer_config_t ledc_timer = {};
  ledc_timer.speed_mode = LEDC_MODE;
  ledc_timer.duty_resolution = LEDC_RESOLUTION;
  ledc_timer.timer_num = LEDC_TIMER;
  ledc_timer.freq_hz = LEDC_FREQUENCY;
  ledc_timer.clk_cfg = LEDC_AUTO_CLK;
  ledc_timer_config(&ledc_timer);

  // 2) LEDC channel 0 → PWM_A (pin 19)
  ledc_channel_config_t channelA = {};
  channelA.channel = LEDC_CHANNEL_A;
  channelA.duty = 0;  // start at 0
  channelA.gpio_num = PWM_A;
  channelA.speed_mode = LEDC_MODE;
  channelA.hpoint = 0;
  channelA.timer_sel = LEDC_TIMER;
  ledc_channel_config(&channelA);

  // 3) LEDC channel 1 → PWM_B (pin 18)
  ledc_channel_config_t channelB = {};
  channelB.channel = LEDC_CHANNEL_B;
  channelB.duty = 0;  // start at 0
  channelB.gpio_num = PWM_B;
  channelB.speed_mode = LEDC_MODE;
  channelB.hpoint = 0;
  channelB.timer_sel = LEDC_TIMER;
  ledc_channel_config(&channelB);
}

//-----------------------------------------------------------------------------
// Write an 8-bit duty to the given LEDC channel
//-----------------------------------------------------------------------------
void writeDuty(ledc_channel_t channel, int duty) {
  if (duty < 0) duty = 0;
  if (duty > 255) duty = 255;
  ledc_set_duty(LEDC_MODE, channel, duty);
  ledc_update_duty(LEDC_MODE, channel);
}

//-----------------------------------------------------------------------------
// Compute pwm_a/pwm_b from `power` and send them to LEDC
//-----------------------------------------------------------------------------
void set_vals() {
  // baseline on B to overcome static friction
  int base_b = 35 + (static_friction_volts / drive_volts) * (PWM_MAX - 1);

  // power∈[0…1] → pwm_b ∈[base_b…255]
  pwm_b = (int)(power * ((PWM_MAX - 1) - base_b) + base_b);

  // Send to LEDC channels:
  writeDuty(LEDC_CHANNEL_A, 0);
  writeDuty(LEDC_CHANNEL_B, pwm_b);

  // (Optional) Read current-sense ADC channels:
  int isns_a_val = analogRead(ISNS_A);
  int isns_b_val = analogRead(ISNS_B);
  // Uncomment to debug:
  // Serial.print("ISNS_A = "); Serial.print(isns_a_val);
  // Serial.print("  ISNS_B = "); Serial.println(isns_b_val);
}

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
  // Loop until we’re reconnected
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

void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println("BTS7960 PWM @ 20 kHz → MQTT Fan Control");

  Serial.println("Scanning networks…");
  int n = WiFi.scanNetworks();
  Serial.printf("Found %d networks\n", n);
  for (int i = 0; i < n; i++) {
    Serial.printf("  %s (%d dBm)\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
  }
  // ————— Connect to Wi-Fi —————
  WiFi.begin(ssid, password);
  Serial.printf("Connecting to Wi-Fi \"%s\" … ", ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Wi-Fi connected, IP = ");
  Serial.println(WiFi.localIP());

  client.setCACert(ca_cert);

  // —————— DS18x20 setup ——————
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

  // Precompute dead_ticks for diagnostics:
  us_period = 1000000.0 / ((float)LEDC_FREQUENCY);  // period in µs
  dead_ticks = PWM_MAX * dead_time_us / us_period;
  Serial.printf("dead_ticks = %.2f\n", dead_ticks);

  // Initialize LEDC hardware for PWM
  setupLEDC();

  // Configure ADC resolution for current sensors
  analogReadResolution(16);
  pinMode(ISNS_A, INPUT);
  pinMode(ISNS_B, INPUT);

  // Enable the two half-bridges
  pinMode(EN_A, OUTPUT);
  pinMode(EN_B, OUTPUT);
  digitalWrite(EN_A, HIGH);
  digitalWrite(EN_B, HIGH);

  // Start with PWM = 0
  set_vals();


  // ————— Setup MQTT client —————
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(callback);
}

void loop() {
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

  // Do a single HTTPS POST for this sensor
  if (client.connect(server, 443)) {
    client.println("POST /particle/log HTTP/1.0");
    client.print("Host: ");
    client.println(server);
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(data.length());
    client.println();
    client.print(data);
    client.println();
    // Wait up to 5 seconds for a response (just to clear the buffer)
    unsigned long timeout = millis() + 5000;
    while (client.connected() && millis() < timeout) {
      while (client.available()) {
        char c = client.read();
        Serial.write(c);
      }
    }
    client.stop();
  } else {
    Serial.println("Failed to connect to server for POST!");
  }


  if (!mqttClient.connected()) {
    reconnectMqtt();
  }

  mqttClient.loop();

  static unsigned long lastPub = 0;
  if (millis() - lastPub > 10000) {
    Serial.println("Sending status update");
    lastPub = millis();
    char buf[32];
    dtostrf(power, 0, 3, buf);  // e.g. "0.250"
    mqttClient.publish("home/fan1/power/status", buf);
  }
}