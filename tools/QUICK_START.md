# Production Flashing - Quick Start

## TL;DR - Flash a Device

```bash
cd tools
./flash_device.sh testboard3
```

That's it! The script will:
- ✅ Build firmware if needed
- ✅ Generate board-specific NVS configuration
- ✅ Erase flash
- ✅ Write bootloader + firmware + configuration
- ✅ Device boots with correct settings

## Common Commands

### Flash Complete Device (Firmware + Config)
```bash
./flash_device.sh testboard3
./flash_device.sh testboard2
./flash_device.sh UnderHouseFan
```

### Update Config Only (Keep Firmware)
```bash
./update_nvs.sh testboard3
```

### Monitor Serial Output
```bash
pio device monitor
# or specify port:
pio device monitor -p /dev/cu.usbmodem101
```

## Available Board Configs

- `testboard2` - TestBoard2 development board
- `testboard3` - TestBoard3 development board
- `UnderHouseFan` - Production UnderHouseFan deployment

## What You'll See After Flashing

```
========== Device Configuration ==========
Device Name: TestBoard3

WiFi Networks (2):
  0: CasaDelVista / 53****rk
  1: BlockbusterFreeWiFi / Le****ie

MQTT:
  Server: 10.10.1.20:1883
  Command Topic: testboard3/fan1/power
  Status Topic: testboard3/fan1/power/status
==========================================

Connecting to WiFi...
Connected to: CasaDelVista
```

## Create New Board Config

```bash
# 1. Copy example
cp config_example.json config_MyBoard.json

# 2. Edit settings
nano config_MyBoard.json

# 3. Flash device
./flash_device.sh MyBoard
```

## Troubleshooting

### "No module named esp_idf_nvs_partition_gen"
```bash
~/.platformio/penv/bin/pip install esp-idf-nvs-partition-gen
```

### "Cannot find serial port"
```bash
# List available ports
ls /dev/cu.*

# Specify port explicitly
./flash_device.sh testboard3 /dev/cu.usbmodem101
```

### Wrong config showing
```bash
# Re-flash NVS
./update_nvs.sh testboard3
```

## Files You Care About

- `config_*.json` - Board configurations (EDIT THESE)
- `flash_device.sh` - Flash complete device
- `update_nvs.sh` - Update config only
- `nvs_*.bin` - Generated binaries (auto-created, don't edit)

## Full Documentation

See [PRODUCTION_WORKFLOW.md](PRODUCTION_WORKFLOW.md) for complete details.
