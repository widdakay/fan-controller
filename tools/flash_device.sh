#!/bin/bash
#
# Production Device Flashing Script
# Usage: ./flash_device.sh <board_name> [serial_port]
#
# Examples:
#   ./flash_device.sh testboard3
#   ./flash_device.sh UnderHouseFan /dev/cu.usbmodem101
#

set -e  # Exit on error

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
PYTHON="${HOME}/.platformio/penv/bin/python3"
ESPTOOL="${HOME}/.platformio/packages/tool-esptoolpy/esptool.py"

# Partition offsets (from default_16MB.csv)
NVS_OFFSET=0x9000
BOOTLOADER_OFFSET=0x1000
PARTITION_TABLE_OFFSET=0x8000
APP_OFFSET=0x10000

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print functions
print_header() {
    echo -e "\n${BLUE}================================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}================================================${NC}\n"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_info() {
    echo -e "${YELLOW}→ $1${NC}"
}

# Usage message
usage() {
    echo "Usage: $0 <board_name> [serial_port]"
    echo ""
    echo "Available boards:"
    for config in "${SCRIPT_DIR}"/config_*.json; do
        board=$(basename "$config" .json | sed 's/config_//')
        echo "  - $board"
    done
    echo ""
    echo "Default serial port: /dev/cu.usbmodem*"
    exit 1
}

# Check arguments
if [ $# -lt 1 ]; then
    usage
fi

BOARD_NAME="$1"
SERIAL_PORT="${2:-/dev/cu.usbmodem*}"

# Expand wildcards in serial port
SERIAL_PORT=$(echo $SERIAL_PORT)

# File paths
CONFIG_FILE="${SCRIPT_DIR}/config_${BOARD_NAME}.json"
NVS_BIN="${SCRIPT_DIR}/nvs_${BOARD_NAME}.bin"
BUILD_DIR="${PROJECT_DIR}/.pio/build/esp32-s3-devkitc-1"
BOOTLOADER="${BUILD_DIR}/bootloader.bin"
PARTITIONS="${BUILD_DIR}/partitions.bin"
FIRMWARE="${BUILD_DIR}/firmware.bin"

# Validate files exist
print_header "Validating Configuration for ${BOARD_NAME}"

if [ ! -f "$CONFIG_FILE" ]; then
    print_error "Config file not found: $CONFIG_FILE"
    exit 1
fi
print_success "Config file: $CONFIG_FILE"

# Build firmware if needed
if [ ! -f "$FIRMWARE" ]; then
    print_info "Firmware not found, building..."
    cd "$PROJECT_DIR"
    pio run
    print_success "Firmware built"
else
    print_success "Firmware: $FIRMWARE"
fi

# Check other build artifacts
if [ ! -f "$BOOTLOADER" ]; then
    print_error "Bootloader not found: $BOOTLOADER"
    print_info "Run 'pio run' first"
    exit 1
fi
print_success "Bootloader: $BOOTLOADER"

if [ ! -f "$PARTITIONS" ]; then
    print_error "Partition table not found: $PARTITIONS"
    exit 1
fi
print_success "Partition table: $PARTITIONS"

# Generate NVS binary
print_header "Generating NVS Configuration"
print_info "Generating NVS from: $CONFIG_FILE"

$PYTHON "${SCRIPT_DIR}/flash_config.py" \
    "$CONFIG_FILE" \
    -o "$NVS_BIN" \
    --size 0x5000

print_success "NVS binary: $NVS_BIN"

# Flash device
print_header "Flashing Device on ${SERIAL_PORT}"

print_info "Erasing flash..."
$PYTHON "$ESPTOOL" \
    --chip esp32s3 \
    --port "$SERIAL_PORT" \
    erase_flash

print_success "Flash erased"

print_info "Writing bootloader, partition table, firmware, and NVS..."
$PYTHON "$ESPTOOL" \
    --chip esp32s3 \
    --port "$SERIAL_PORT" \
    --baud 460800 \
    write_flash \
    --flash_mode dio \
    --flash_size 16MB \
    ${BOOTLOADER_OFFSET} "$BOOTLOADER" \
    ${PARTITION_TABLE_OFFSET} "$PARTITIONS" \
    ${APP_OFFSET} "$FIRMWARE" \
    ${NVS_OFFSET} "$NVS_BIN"

print_success "Device flashed successfully!"

print_header "Summary"
echo "Board: ${BOARD_NAME}"
echo "Config: ${CONFIG_FILE}"
echo "NVS: ${NVS_BIN}"
echo "Port: ${SERIAL_PORT}"
echo ""
print_success "Device is ready for deployment!"
echo ""
print_info "To monitor serial output: pio device monitor -p ${SERIAL_PORT}"
