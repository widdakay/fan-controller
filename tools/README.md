# ESP32 Device Configuration Tool

Python tool for configuring ESP32 devices over MQTT using JSON configuration files.

## Prerequisites

Install the required Python package:

```bash
pip install paho-mqtt
```

## Quick Start

1. **Create a configuration file** (see `config_example.json`)

2. **Run the configuration tool:**
   ```bash
   python configure_device.py config_example.json
   ```

3. **Restart your ESP32 device** for most changes to take effect

## Configuration File Format

```json
{
  "device_name": "UnderHouseFan",
  "topic_prefix": "home",

  "wifi_networks": [
    {
      "ssid": "YourSSID",
      "password": "YourPassword"
    }
  ],

  "mqtt_server": "10.10.1.20",
  "mqtt_port": 1883,

  "mqtt_topics": {
    "command": "home/fan1/power",
    "status": "home/fan1/power/status"
  },

  "api_endpoints": {
    "influxdb": "https://your-server.com/particle/log",
    "firmware": "https://your-server.com/particle/fw/update"
  },

  "base_topic": "home/fan1"
}
```

### Configuration Fields

| Field | Required | Description |
|-------|----------|-------------|
| `device_name` | Yes | Unique name for the device (used for OTA hostname) |
| `topic_prefix` | No | MQTT topic prefix (defaults to device_name) |
| `wifi_networks` | Yes | Array of WiFi credentials (up to 5 networks) |
| `mqtt_server` | Yes | MQTT broker IP address or hostname |
| `mqtt_port` | No | MQTT broker port (default: 1883) |
| `mqtt_topics.command` | Yes | Topic to receive power commands |
| `mqtt_topics.status` | Yes | Topic to publish power status |
| `api_endpoints.influxdb` | Yes | InfluxDB telemetry endpoint |
| `api_endpoints.firmware` | Yes | Firmware update check endpoint |
| `base_topic` | Yes | Base MQTT topic for the device |

## Usage Examples

### Basic Configuration

Configure a device using default broker (10.10.1.20):

```bash
python configure_device.py config_example.json
```

### Specify MQTT Broker

```bash
python configure_device.py config_example.json --broker 192.168.1.100 --port 1883
```

### Override Device Name

Useful when configuring multiple devices with the same config template:

```bash
python configure_device.py config_example.json --device TestBoard1
```

### Print Current Configuration

Request the device to print its current configuration to serial output:

```bash
python configure_device.py --print-config --device TestBoard3 --broker 10.10.1.20
```

Note: Check the device's serial monitor to see the printed configuration.

### Reset to Factory Defaults

```bash
python configure_device.py --reset-config --device TestBoard3 --broker 10.10.1.20
```

⚠️ This will erase all saved configuration and restore defaults. The device will need to be restarted.

### Custom Timeout

For slower networks or devices:

```bash
python configure_device.py config.json --timeout 10
```

## Command-Line Options

```
usage: configure_device.py [-h] [--broker BROKER] [--port PORT]
                          [--device DEVICE] [--timeout TIMEOUT]
                          [--print-config] [--reset-config]
                          [config_file]

positional arguments:
  config_file        JSON configuration file

optional arguments:
  -h, --help         show this help message and exit
  --broker BROKER    MQTT broker address (default: 10.10.1.20)
  --port PORT        MQTT broker port (default: 1883)
  --device DEVICE    Device name/topic prefix (overrides config file)
  --timeout TIMEOUT  Response timeout in seconds (default: 5)
  --print-config     Only print device configuration (requires --device)
  --reset-config     Reset device to defaults (requires --device)
```

## MQTT Topics

The tool uses the following MQTT topic structure:

- **Configuration commands**: `<base_topic>/config`
- **Configuration status**: `<base_topic>/config/status`

For example, with `base_topic` = `"home/fan1"`:
- Commands sent to: `home/fan1/config`
- Responses received on: `home/fan1/config/status`

## Configuration Commands

The tool sends these commands to the device:

### 1. Set Device Name
```json
{"cmd": "set_device_name", "name": "UnderHouseFan"}
```

### 2. Set WiFi Credentials
```json
{
  "cmd": "set_wifi",
  "index": 0,
  "ssid": "YourSSID",
  "password": "YourPassword"
}
```

Supports up to 5 WiFi networks (index 0-4). Device will connect to the strongest available network.

### 3. Set MQTT Server
```json
{
  "cmd": "set_mqtt_server",
  "server": "10.10.1.20",
  "port": 1883
}
```

### 4. Set MQTT Topics
```json
{
  "cmd": "set_mqtt_topics",
  "command": "home/fan1/power",
  "status": "home/fan1/power/status"
}
```

### 5. Set API Endpoints
```json
{
  "cmd": "set_api_endpoints",
  "influxdb": "https://data.yoerik.com/particle/log",
  "firmware": "https://data.yoerik.com/particle/fw/update"
}
```

### 6. Print Config
```json
{"cmd": "print_config"}
```

Prints current configuration to device serial output.

### 7. Reset Config
```json
{"cmd": "reset_config"}
```

Resets all configuration to factory defaults.

## Expected Output

Successful configuration:

```
✓ Connected to MQTT broker at 10.10.1.20:1883

============================================================
Configuring device: UnderHouseFan
Config topic: home/fan1/config
Status topic: home/fan1/config/status
============================================================

[1/6] Setting device name to 'UnderHouseFan'...
  Response: OK: Device name updated
[2/6] Setting WiFi network 0: 'CasaDelVista'...
  Response: OK: WiFi updated, restart required
[3/6] Setting MQTT server to '10.10.1.20'...
  Response: OK: MQTT server updated, restart required
[4/6] Setting MQTT topics...
  Response: OK: Topics updated, restart required
[5/6] Setting API endpoints...
  Response: OK: API endpoints updated, restart required
[6/6] Requesting configuration printout...
  Response: OK: Config printed to serial

============================================================
✓ Configuration completed successfully!
⚠ Most changes require a device restart to take effect
============================================================

✓ Disconnected from MQTT broker
```

## Workflow for Multiple Devices

1. Create configuration templates for each device type
2. Use the `--device` flag to specify which device to configure
3. The tool will apply the configuration and report status

Example workflow:

```bash
# Configure first device
python configure_device.py config_example.json --device UnderHouseFan

# Configure second device with different name
python configure_device.py config_example.json --device GarageFan

# Configure test board
python configure_device.py config_testboard3.json
```

## Troubleshooting

### No response received (timeout)

**Problem**: Device doesn't respond to configuration commands

**Solutions**:
- Check that device is powered on and connected to WiFi
- Verify MQTT broker address is correct
- Ensure device is subscribed to the config topic
- Check device serial output for errors
- Increase timeout: `--timeout 10`

### Connection refused

**Problem**: Cannot connect to MQTT broker

**Solutions**:
- Verify broker IP address and port
- Check that broker is running: `mosquitto -v`
- Test broker connectivity: `mosquitto_pub -h 10.10.1.20 -t test -m "hello"`

### Invalid JSON error

**Problem**: Configuration file has syntax errors

**Solutions**:
- Validate JSON using online tool: https://jsonlint.com
- Check for missing commas, quotes, or brackets
- Remove trailing commas in arrays/objects

## Testing MQTT Manually

You can test the configuration manually using `mosquitto_pub`:

```bash
# Print config
mosquitto_pub -h 10.10.1.20 -t "home/fan1/config" -m '{"cmd":"print_config"}'

# Set device name
mosquitto_pub -h 10.10.1.20 -t "home/fan1/config" \
  -m '{"cmd":"set_device_name","name":"MyDevice"}'

# Subscribe to responses
mosquitto_sub -h 10.10.1.20 -t "home/fan1/config/status"
```

## Security Notes

⚠️ **Important Security Considerations:**

1. **WiFi passwords are stored in plain text** in the JSON file
   - Keep configuration files secure
   - Add `*.json` to `.gitignore` if storing in version control
   - Consider using environment variables for sensitive data

2. **MQTT traffic is unencrypted** by default
   - Use MQTT over TLS/SSL for production
   - Implement MQTT authentication
   - Restrict broker access by IP address

3. **Configuration commands are unauthenticated**
   - Anyone with MQTT access can reconfigure devices
   - Use MQTT ACLs to restrict topic access
   - Consider implementing a PIN/password in config commands

## Advanced Usage

### Batch Configuration Script

Create a shell script to configure multiple devices:

```bash
#!/bin/bash
# configure_all.sh

devices=("UnderHouseFan" "GarageFan" "AtticFan")
broker="10.10.1.20"

for device in "${devices[@]}"; do
    echo "Configuring $device..."
    python configure_device.py "config_${device}.json" \
        --broker "$broker" \
        --device "$device"
    echo "Waiting 5 seconds..."
    sleep 5
done

echo "All devices configured!"
```

### Python Integration

Use the tool programmatically in your own scripts:

```python
from configure_device import DeviceConfigurator

# Create configurator
config = DeviceConfigurator("10.10.1.20", 1883)
config.connect()

# Send custom command
config.send_command(
    "home/fan1/config",
    {"cmd": "set_device_name", "name": "MyFan"},
    "home/fan1/config/status"
)

config.disconnect()
```

## License

This tool is part of the ESP32 HVAC System project.
