#include "config.h"
#include "pwm.h"
#include "sensors.h"
#include "mqtt.h"
#include "https.h"

// Global variable definitions
float dead_time_us = 6.8;  // µs of dead-time
float drive_volts = 12.0;
float static_friction_volts = 0.5;
volatile float power = 0.0;
int pwm_a = 0;
int pwm_b = 0;
float us_period;
float dead_ticks;

const char* ssid = "CasaIOT";
const char* password = "LetMeIOT";
const char* mqtt_server = "10.10.1.20";
const int mqtt_port = 1883;
const char* power_topic = "home/fan1/power";
WiFiClient espClient;
PubSubClient mqttClient(espClient);

const char* ca_cert =
  "-----BEGIN CERTIFICATE-----\n"
  "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\n"
  "-----END CERTIFICATE-----\n";

const char server[] = "data.yoerik.com";
WiFiClientSecure client;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
uint8_t deviceCount = 0;

// Forward declarations
void reconnectMqtt();
void callback(char* topic, byte* payload, unsigned int length);

//-----------------------------------------------------------------------------
// Configure LEDC Timer 0 for 20 kHz, 8-bit resolution
//-----------------------------------------------------------------------------









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
  setupSensors();

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
  String data = getTemperatureData();
  sendHttpsData(data);

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