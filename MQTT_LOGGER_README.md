# ESP32 MQTT Logger Monitor

This script monitors ESP32 device logs via MQTT in real-time.

## Prerequisites

Install the required Python package:
```bash
pip install paho-mqtt
```

## Usage

### Basic usage (connects to localhost):
```bash
python mqtt_logger.py
```

### Specify MQTT broker:
```bash
python mqtt_logger.py your-mqtt-broker-ip
```

### Specify broker and port:
```bash
python mqtt_logger.py your-mqtt-broker-ip 1883
```

### Specify custom topic:
```bash
python mqtt_logger.py your-mqtt-broker-ip 1883 your-device/logs
```

## Default Settings

- **Broker**: localhost (change to your MQTT server IP)
- **Port**: 1883 (standard MQTT port)
- **Topic**: testboard3/logs (device logs topic)

## Expected Output

When the ESP32 device is running with MQTT logging enabled, you should see:

```
🚀 ESP32 MQTT Logger Subscriber
📍 Broker: 192.168.1.100:1883
📡 Topic: testboard3/logs
────────────────────────────────────────────────────────────────────────────────
✅ Connected to MQTT broker 192.168.1.100:1883
📡 Subscribed to topic: testboard3/logs

🎧 Listening for ESP32 device logs...
Press Ctrl+C to stop

[14:23:45.123] 📨 testboard3/logs: [123456][INFO][Application.cpp:25] System initialized successfully
[14:23:46.456] 📨 testboard3/logs: [123457][ERROR][WiFiManager.cpp:45] WiFi connection failed
[14:23:47.789] 📨 testboard3/logs: [123458][DEBUG][SensorDriver.cpp:89] ADC reading: 2.45V
```

## Troubleshooting

1. **Connection failed**: Make sure the MQTT broker is running and accessible
2. **No messages**: Ensure the ESP32 device is connected to the same MQTT broker and has MQTT logging enabled
3. **Wrong topic**: Check that the device is publishing to the expected topic (default: `{deviceName}/logs`)

## Log Format

The logs follow this format:
```
[timestamp][LEVEL][file:line] message
```

- **timestamp**: Milliseconds since ESP32 boot
- **LEVEL**: DEBUG, INFO, WARN, ERROR
- **file:line**: Source file and line number
- **message**: The actual log message