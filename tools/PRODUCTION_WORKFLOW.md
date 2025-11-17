# Production Workflow Guide

This guide describes the production workflow for flashing multiple ESP32 devices with board-specific configurations.

## Overview

The production process follows this pattern:

1. **Build firmware once** with sensible defaults (generic firmware)
2. **Flash each device** with board-specific NVS configuration
3. **Verify** device boots with correct configuration

## Architecture

### Configuration System

- **Compile-time defaults** (`include/Config.hpp`, `include/SecureConfig.hpp`)
  - Used as fallback when NVS is empty
  - Hardware pin assignments (never change at runtime)
  - Task timing intervals

- **Runtime configuration** (NVS partition)
  - Device name, WiFi credentials, MQTT settings, API endpoints
  - Loaded from NVS on boot via `ConfigManager`
  - Can be updated via MQTT at runtime
  - **Pre-programmed** from JSON config files for production

### Partition Layout

From `default_16MB.csv`:

| Partition | Offset | Size | Purpose |
|-----------|--------|------|---------|
| Bootloader | 0x1000 | - | ESP32 bootloader |
| Partition Table | 0x8000 | 0x1000 | Partition map |
| **NVS** | **0x9000** | **0x5000** (20KB) | **Configuration storage** |
| OTA Data | 0xe000 | 0x2000 | OTA status |
| App0 | 0x10000 | 0x640000 | Firmware slot 0 |
| App1 | 0x650000 | 0x640000 | Firmware slot 1 (OTA) |
| SPIFFS | 0xc90000 | 0x360000 | File system |

## Configuration Files

Board-specific JSON files in `tools/`:

- `config_testboard2.json` - TestBoard2 development board
- `config_testboard3.json` - TestBoard3 development board
- `config_UnderHouseFan.json` - Production deployment
- `config_example.json` - Template for new boards

### Configuration File Format

```json
{
  "_comment": "Board description",

  "device_name": "UniqueBoardName",
  "topic_prefix": "mqtt/topic/prefix",

  "wifi_networks": [
    {
      "ssid": "NetworkSSID",
      "password": "wifi_password"
    }
  ],

  "mqtt_server": "10.10.1.20",
  "mqtt_port": 1883,

  "mqtt_topics": {
    "command": "device/fan1/power",
    "status": "device/fan1/power/status"
  },

  "api_endpoints": {
    "influxdb": "https://data.example.com/particle/log",
    "firmware": "https://data.example.com/particle/fw/update"
  }
}
```

## Production Scripts

### 1. `flash_device.sh` - Complete Device Flash

**Purpose**: Flash a complete device from scratch (firmware + configuration)

**Usage**:
```bash
cd tools
./flash_device.sh <board_name> [serial_port]
```

**Examples**:
```bash
# Flash TestBoard3 on default USB port
./flash_device.sh testboard3

# Flash UnderHouseFan on specific port
./flash_device.sh UnderHouseFan /dev/cu.usbmodem101
```

**What it does**:
1. Validates config file exists
2. Builds firmware (if not already built)
3. Generates NVS binary from JSON config
4. **Erases entire flash**
5. Writes bootloader, partition table, firmware, AND NVS
6. Device boots with board-specific configuration

**When to use**:
- Initial device programming
- Completely reprogramming a device
- Production line flashing

---

### 2. `update_nvs.sh` - Update Configuration Only

**Purpose**: Update NVS configuration without reflashing firmware

**Usage**:
```bash
cd tools
./update_nvs.sh <board_name> [serial_port]
```

**Examples**:
```bash
# Update TestBoard3 config
./update_nvs.sh testboard3

# Update UnderHouseFan on specific port
./update_nvs.sh UnderHouseFan /dev/cu.usbmodem101
```

**What it does**:
1. Generates NVS binary from JSON config
2. **Erases ONLY the NVS partition** (firmware untouched)
3. Writes new NVS configuration
4. Device reboots with new configuration

**When to use**:
- Changing WiFi credentials
- Updating MQTT server/topics
- Repurposing a device for different deployment

**⚠️ Warning**: Erases any runtime configuration changes made via MQTT!

---

### 3. `flash_config.py` - Low-Level NVS Generation

**Purpose**: Generate NVS binary from JSON (used by other scripts)

**Usage**:
```bash
# Generate NVS binary only
python3 flash_config.py config_testboard3.json -o nvs_testboard3.bin

# Generate and flash NVS
python3 flash_config.py config_testboard3.json \
    -o nvs_testboard3.bin \
    --flash \
    --port /dev/cu.usbmodem101
```

**Parameters**:
- `--size` - Partition size (default: 0x5000 = 20KB)
- `--offset` - Flash offset (default: 0x9000)
- `--flash` - Flash after generation
- `--port` - Serial port

## Production Workflow

### Scenario 1: Production Line (Multiple Identical Devices)

```bash
# 1. Build firmware once
cd /path/to/ESPCode
pio run

# 2. Pre-generate NVS binary
cd tools
~/.platformio/penv/bin/python3 flash_config.py \
    config_UnderHouseFan.json \
    -o nvs_UnderHouseFan.bin

# 3. For each device:
./flash_device.sh UnderHouseFan /dev/cu.usbmodem101
# ... connect next device ...
./flash_device.sh UnderHouseFan /dev/cu.usbmodem101
```

---

### Scenario 2: Different Board Configurations

```bash
# Flash TestBoard2
./flash_device.sh testboard2

# Flash TestBoard3
./flash_device.sh testboard3

# Flash production device
./flash_device.sh UnderHouseFan
```

---

### Scenario 3: Update Configuration on Deployed Device

```bash
# 1. Edit the config file
nano config_UnderHouseFan.json

# 2. Update NVS only (keeps firmware)
./update_nvs.sh UnderHouseFan
```

---

### Scenario 4: Create New Board Configuration

```bash
# 1. Copy template
cp config_example.json config_MyNewBoard.json

# 2. Edit configuration
nano config_MyNewBoard.json
# Update: device_name, wifi_networks, mqtt_server, mqtt_topics

# 3. Flash device
./flash_device.sh MyNewBoard
```

## Verification

After flashing, monitor serial output to verify configuration:

```bash
pio device monitor
```

**Expected output**:
```
=== ESP32 Air Quality Controller ===
Firmware: 1.0.0
Chip ID: [chip_id]

[ConfigManager] Configuration loaded from NVS

========== Device Configuration ==========
Device Name: TestBoard3

WiFi Networks (2):
  0: CasaDelVista / 53****rk
  1: BlockbusterFreeWiFi / Le****ie

MQTT:
  Server: 10.10.1.20:1883
  Command Topic: testboard3/fan1/power
  Status Topic: testboard3/fan1/power/status

API Endpoints:
  InfluxDB: https://data.yoerik.com/particle/log
  FW Update: https://data.yoerik.com/particle/fw/update
==========================================

Connecting to WiFi...
Connected to: CasaDelVista
IP Address: 192.168.1.xxx
```

## Runtime Configuration Updates

Devices can be reconfigured via MQTT without reflashing:

### Update Device Name
```bash
mosquitto_pub -t testboard3/fan1/config \
  -m '{"cmd":"set_device_name","name":"NewName"}'
```

### Update WiFi Credentials
```bash
mosquitto_pub -t testboard3/fan1/config \
  -m '{"cmd":"set_wifi","index":0,"ssid":"NewSSID","password":"newpassword"}'
```

### Update MQTT Server
```bash
mosquitto_pub -t testboard3/fan1/config \
  -m '{"cmd":"set_mqtt_server","server":"192.168.1.100","port":1883}'
```

### Print Current Config
```bash
mosquitto_pub -t testboard3/fan1/config \
  -m '{"cmd":"print_config"}'
```

### Reset to Compiled Defaults
```bash
mosquitto_pub -t testboard3/fan1/config \
  -m '{"cmd":"reset_config"}'
```

**Note**: Most changes require a device restart to take effect.

## Troubleshooting

### NVS Generation Fails

**Error**: `No module named esp_idf_nvs_partition_gen`

**Solution**:
```bash
~/.platformio/penv/bin/pip install esp-idf-nvs-partition-gen
```

---

### Device Not Recognized

**Error**: `Cannot access /dev/cu.usbmodem*`

**Check**:
1. USB cable connected
2. Device powered on
3. USB drivers installed
4. Check available ports: `ls /dev/cu.*`

---

### Wrong Configuration Loaded

**Scenario**: Device shows different config than expected

**Check**:
1. Was NVS flashed AFTER firmware?
2. Did device reboot after NVS flash?
3. Is the correct JSON file being used?

**Solution**: Re-flash NVS:
```bash
./update_nvs.sh <board_name>
```

---

### Device Shows Defaults Instead of NVS Config

**Scenario**: Device uses TestBoard3 from Config.hpp instead of NVS

**Cause**: NVS partition is empty or corrupted

**Solution**:
```bash
# Re-flash NVS
./update_nvs.sh <board_name>
```

---

### Configuration Won't Update via MQTT

**Check**:
1. MQTT broker accessible
2. Correct topic format: `<device_prefix>/fan1/config`
3. Valid JSON in payload
4. Device subscribed to config topic

**Debug**:
```bash
# Watch MQTT traffic
mosquitto_sub -v -t '#'
```

## File Reference

### Generated Files (tools/)

- `nvs_testboard2.bin` - NVS binary for TestBoard2
- `nvs_testboard3.bin` - NVS binary for TestBoard3
- `nvs_UnderHouseFan.bin` - NVS binary for UnderHouseFan
- `nvs_*.bin` - Auto-generated from JSON configs

### Build Artifacts (.pio/build/esp32-s3-devkitc-1/)

- `bootloader.bin` - ESP32 bootloader
- `partitions.bin` - Partition table
- `firmware.bin` - Main application firmware

## Best Practices

1. **Version Control**
   - Commit JSON config files to git
   - DO NOT commit `nvs_*.bin` files (regenerate as needed)
   - Tag firmware releases

2. **Device Naming**
   - Use descriptive, unique device names
   - Include location or purpose: `Garage_Fan`, `Basement_Monitor`
   - Avoid generic names: `ESP32`, `Test`

3. **WiFi Credentials**
   - Support multiple networks for redundancy
   - Order by preference (strongest signal first)
   - Include backup network for failover

4. **Production Security**
   - Use strong WiFi passwords
   - Change default MQTT credentials
   - Consider MQTT TLS for production

5. **Testing**
   - Always verify configuration after flashing
   - Test MQTT connectivity
   - Verify sensor readings
   - Test OTA updates

## Advanced Topics

### Custom Partition Tables

To change NVS partition size/offset:

1. Copy partition table:
```bash
cp ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/default_16MB.csv \
   custom_partitions.csv
```

2. Edit partition offsets

3. Update `platformio.ini`:
```ini
board_build.partitions = custom_partitions.csv
```

4. Update `flash_config.py` defaults:
```python
DEFAULT_SIZE = 0x6000  # New size
DEFAULT_OFFSET = 0x9000  # New offset
```

---

### Encrypted NVS

For production security, enable NVS encryption:

```bash
python3 flash_config.py config.json \
    -o nvs.bin \
    --encrypt \
    --key nvs_encryption_key.bin
```

Requires ESP32 eFuse programming for encryption keys.

## Summary

| Task | Command |
|------|---------|
| Flash complete device | `./flash_device.sh <board>` |
| Update config only | `./update_nvs.sh <board>` |
| Generate NVS binary | `python3 flash_config.py <config.json> -o <output.bin>` |
| Monitor device | `pio device monitor` |
| Build firmware | `pio run` |
| Erase device | `pio run --target erase` |

For more information, see:
- [README.md](README.md) - General project documentation
- [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md) - Detailed config reference
- [/CLAUDE.md](/CLAUDE.md) - Development guide
