# ESP32 Device Configuration Guide

This guide explains all the ways to configure your ESP32 devices.

## Table of Contents

1. [Initial Configuration (Pre-Flash)](#initial-configuration-pre-flash)
2. [Runtime Configuration (MQTT)](#runtime-configuration-mqtt)
3. [Comparison of Methods](#comparison-of-methods)
4. [Workflow Examples](#workflow-examples)

---

## Initial Configuration (Pre-Flash)

Configure devices **before** they first boot by pre-programming the NVS partition.

### Advantages
- ✅ Device is fully configured on first boot
- ✅ No need for WiFi/MQTT to be working initially
- ✅ Perfect for production deployment
- ✅ Same firmware binary works for all devices
- ✅ Configuration survives firmware updates

### Method 1: Generate and Flash NVS Partition

**Step 1: Create configuration file** (`my_device.json`):

```json
{
  "device_name": "UnderHouseFan",
  "wifi_networks": [
    {"ssid": "YourSSID", "password": "YourPassword"}
  ],
  "mqtt_server": "10.10.1.20",
  "mqtt_port": 1883,
  "mqtt_topics": {
    "command": "home/fan1/power",
    "status": "home/fan1/power/status"
  },
  "api_endpoints": {
    "influxdb": "https://data.yoerik.com/particle/log",
    "firmware": "https://data.yoerik.com/particle/fw/update"
  },
  "base_topic": "home/fan1"
}
```

**Step 2: Generate NVS partition binary:**

```bash
python generate_nvs_partition.py my_device.json -o nvs_my_device.bin
```

**Step 3a: Flash firmware AND NVS partition together:**

```bash
# Flash firmware first
pio run --target upload --upload-port /dev/ttyUSB0

# Then flash NVS partition
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
  write_flash 0x9000 nvs_my_device.bin
```

**Step 3b: Or use the automated flash option:**

```bash
python generate_nvs_partition.py my_device.json \
  --flash --port /dev/ttyUSB0
```

### Method 2: Batch Flash Multiple Devices

Create a script to flash multiple devices with different configurations:

```bash
#!/bin/bash
# flash_devices.sh

devices=("UnderHouseFan" "GarageFan" "AtticFan")

for device in "${devices[@]}"; do
    echo "==========================================="
    echo "Preparing ${device}..."
    echo "==========================================="

    # Generate NVS partition for this device
    python generate_nvs_partition.py \
        "config_${device}.json" \
        -o "nvs_${device}.bin"

    echo ""
    echo "Connect ${device} and press Enter..."
    read

    # Flash firmware (only once, same for all devices)
    pio run --target upload --upload-port /dev/ttyUSB0

    # Flash device-specific NVS partition
    esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
        write_flash 0x9000 "nvs_${device}.bin"

    echo ""
    echo "✓ ${device} configured!"
    echo ""
done

echo "All devices configured!"
```

### Understanding NVS Partition Location

The NVS partition is located at flash offset `0x9000` by default. This is defined in your `partitions.csv`:

```csv
# Name,   Type, SubType, Offset,  Size
nvs,      data, nvs,     0x9000,  0x6000
```

- **Offset**: `0x9000` (36KB from start of flash)
- **Size**: `0x6000` (24KB)

When you flash the NVS partition, it pre-populates this area with your configuration.

---

## Runtime Configuration (MQTT)

Configure devices **after** they're running using MQTT commands.

### Advantages
- ✅ No physical access needed
- ✅ Can update configuration remotely
- ✅ Good for testing and development
- ✅ Can change configuration without reflashing

### Disadvantages
- ❌ Requires device to already be connected to WiFi/MQTT
- ❌ Most changes require restart to take effect

### Using the Configuration Tool

```bash
# Configure a running device
python configure_device.py config.json --broker 10.10.1.20

# Print current configuration
python configure_device.py --print-config \
  --device UnderHouseFan --broker 10.10.1.20

# Reset to defaults
python configure_device.py --reset-config \
  --device UnderHouseFan --broker 10.10.1.20
```

### Manual MQTT Commands

You can also send commands directly via MQTT:

```bash
# Set device name
mosquitto_pub -h 10.10.1.20 -t "home/fan1/config" \
  -m '{"cmd":"set_device_name","name":"MyDevice"}'

# Set WiFi credentials
mosquitto_pub -h 10.10.1.20 -t "home/fan1/config" \
  -m '{"cmd":"set_wifi","index":0,"ssid":"MySSID","password":"MyPass"}'

# Print configuration
mosquitto_pub -h 10.10.1.20 -t "home/fan1/config" \
  -m '{"cmd":"print_config"}'

# Listen for responses
mosquitto_sub -h 10.10.1.20 -t "home/fan1/config/status"
```

---

## Comparison of Methods

| Feature | Pre-Flash NVS | MQTT Runtime |
|---------|---------------|--------------|
| **When to use** | First deployment | Updates/changes |
| **Requires WiFi** | No | Yes |
| **Requires MQTT** | No | Yes |
| **Physical access** | Yes (USB cable) | No |
| **Survives firmware update** | Yes | Yes |
| **Same firmware for all devices** | Yes | Yes |
| **Configuration effort** | Higher initial | Lower initial |
| **Best for** | Production | Development |

---

## Workflow Examples

### Workflow 1: New Device Deployment (Production)

**Goal**: Deploy a new device with pre-configured settings

```bash
# 1. Create device configuration
cat > config_device1.json <<EOF
{
  "device_name": "UnderHouseFan",
  "wifi_networks": [
    {"ssid": "HomeNetwork", "password": "secret123"}
  ],
  "mqtt_server": "10.10.1.20",
  "mqtt_port": 1883,
  "mqtt_topics": {
    "command": "home/fan1/power",
    "status": "home/fan1/power/status"
  },
  "api_endpoints": {
    "influxdb": "https://data.yoerik.com/particle/log",
    "firmware": "https://data.yoerik.com/particle/fw/update"
  },
  "base_topic": "home/fan1"
}
EOF

# 2. Generate NVS partition
python generate_nvs_partition.py config_device1.json -o nvs_device1.bin

# 3. Flash firmware and NVS
pio run --target upload --upload-port /dev/ttyUSB0
esptool.py --chip esp32s3 --port /dev/ttyUSB0 write_flash 0x9000 nvs_device1.bin

# 4. Device boots fully configured!
```

### Workflow 2: Update Configuration on Running Device

**Goal**: Change MQTT topics on a device that's already deployed

```bash
# Update via MQTT (device stays online)
python configure_device.py updated_config.json --broker 10.10.1.20

# Device needs restart to apply changes
# Restart remotely via MQTT or power cycle
```

### Workflow 3: Development/Testing

**Goal**: Quickly iterate on configuration during development

```bash
# Method A: Use defaults on first boot, configure via MQTT
pio run --target upload
# Device boots with defaults (but WiFi may not connect)
# Configure via MQTT once connected

# Method B: Pre-flash for faster iteration
python generate_nvs_partition.py dev_config.json --flash --port /dev/ttyUSB0
pio run --target upload
# Device boots fully configured
```

### Workflow 4: Manufacturing (Multiple Devices)

**Goal**: Program 10 identical devices efficiently

```bash
# 1. Build firmware ONCE
pio run

# 2. Prepare firmware binary for repeated flashing
cp .pio/build/esp32-s3-devkitc-1/firmware.bin production_firmware.bin

# 3. Create NVS partition for each device
for i in {1..10}; do
    # Update device name in config
    sed "s/DEVICE_NAME/Device${i}/" config_template.json > "config_dev${i}.json"

    # Generate NVS partition
    python generate_nvs_partition.py "config_dev${i}.json" -o "nvs_dev${i}.bin"
done

# 4. Flash each device (connect one at a time)
for i in {1..10}; do
    echo "Connect Device ${i} and press Enter..."
    read

    # Flash firmware
    esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
        write_flash 0x10000 production_firmware.bin

    # Flash device-specific NVS
    esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
        write_flash 0x9000 "nvs_dev${i}.bin"

    echo "✓ Device ${i} complete!"
done
```

### Workflow 5: Firmware Update (Preserve Config)

**Goal**: Update firmware without losing configuration

```bash
# Configuration is in NVS partition (0x9000)
# Firmware is at partition offset (0x10000)
# They don't overlap, so firmware updates preserve config!

# Just flash new firmware
pio run --target upload

# Or via OTA
# Device checks for updates automatically
# Or trigger via MQTT: {"cmd":"check_update"}

# Configuration is preserved!
```

---

## Troubleshooting

### NVS Partition Won't Flash

**Problem**: `esptool.py` fails to flash NVS partition

**Solutions**:
```bash
# Check partition table
esptool.py --chip esp32s3 --port /dev/ttyUSB0 read_flash 0x8000 0x1000 partitions.bin
python -m esptool partition_table partitions.bin

# Verify offset matches your partition table
# Default is 0x9000 for NVS

# Try slower baud rate
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 115200 \
    write_flash 0x9000 nvs.bin
```

### Device Boots with Defaults After Flashing NVS

**Problem**: Configuration not being loaded from NVS

**Causes**:
1. NVS partition flashed to wrong offset
2. Partition table doesn't match
3. NVS binary is corrupt

**Solutions**:
```bash
# 1. Read back NVS partition to verify
esptool.py --chip esp32s3 --port /dev/ttyUSB0 \
    read_flash 0x9000 0x6000 nvs_readback.bin

# 2. Compare with original
diff nvs.bin nvs_readback.bin

# 3. Check device serial output for NVS errors
pio device monitor

# 4. Reflash both bootloader and NVS
esptool.py --chip esp32s3 --port /dev/ttyUSB0 erase_flash
pio run --target upload
esptool.py --chip esp32s3 --port /dev/ttyUSB0 write_flash 0x9000 nvs.bin
```

### nvs_partition_gen.py Not Found

**Problem**: Script can't find ESP-IDF NVS partition generator

**Solutions**:

**Option 1: Install ESP-IDF**
```bash
# Install ESP-IDF
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
. ./export.sh

# Script will now be in PATH
which nvs_partition_gen.py
```

**Option 2: Use PlatformIO's Copy**
```bash
# Find the script
find ~/.platformio -name "nvs_partition_gen.py"

# Copy to tools directory
cp ~/.platformio/packages/framework-arduinoespressif32/tools/nvs_partition_generator/nvs_partition_gen.py .

# Update generate_nvs_partition.py to use local copy
```

**Option 3: Manual CSV to Binary**
```bash
# Generate just CSV
python generate_nvs_partition.py config.json --csv-only -o config.csv

# Download nvs_partition_gen.py
wget https://raw.githubusercontent.com/espressif/esp-idf/master/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py

# Generate binary manually
python nvs_partition_gen.py generate config.csv nvs.bin 0x6000
```

---

## Security Considerations

### NVS Partition Contains Sensitive Data

The NVS partition binary contains:
- WiFi passwords (plain text)
- MQTT server addresses
- API endpoints

**Best Practices**:
1. **Don't commit NVS binaries to version control**
   ```bash
   # Add to .gitignore
   echo "*.bin" >> .gitignore
   echo "nvs_*.bin" >> .gitignore
   ```

2. **Encrypt NVS partition** (ESP32 feature):
   ```
   # Enable NVS encryption in platformio.ini
   board_build.partitions = partitions_encrypted.csv
   ```

3. **Secure configuration files**:
   ```bash
   # Restrict permissions
   chmod 600 config_*.json

   # Use environment variables
   export WIFI_PASSWORD="secret"
   # Update script to read from env vars
   ```

4. **Use secure MQTT**:
   - Enable MQTT over TLS
   - Use MQTT authentication
   - Implement ACLs

---

## Summary

### For First-Time Setup: Use Pre-Flash NVS
```bash
python generate_nvs_partition.py config.json --flash --port /dev/ttyUSB0
```

### For Updates: Use MQTT Configuration
```bash
python configure_device.py config.json --broker 10.10.1.20
```

### For Production: Combine Both
1. Flash firmware + NVS partition for initial deployment
2. Use MQTT for minor config updates
3. Re-flash NVS only when needed (major reconfig)

Choose the method that best fits your workflow!
