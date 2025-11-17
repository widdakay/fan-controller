#!/bin/bash
#
# Update NVS Configuration Only
# Usage: ./update_nvs.sh <board_name> [serial_port]
#
# This script updates ONLY the NVS partition without touching the firmware.
# Use this when you want to change configuration on an already-programmed device.
#
# Examples:
#   ./update_nvs.sh testboard3
#   ./update_nvs.sh UnderHouseFan /dev/cu.usbmodem101
#

set -e  # Exit on error

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON="${HOME}/.platformio/penv/bin/python3"
ESPTOOL="${HOME}/.platformio/packages/tool-esptoolpy/esptool.py"

# Partition offset (from default_16MB.csv)
NVS_OFFSET=0x9000

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

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
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

# Validate files exist
print_header "Updating NVS for ${BOARD_NAME}"

if [ ! -f "$CONFIG_FILE" ]; then
    print_error "Config file not found: $CONFIG_FILE"
    exit 1
fi
print_success "Config file: $CONFIG_FILE"

# Generate NVS binary
print_info "Generating NVS from: $CONFIG_FILE"

$PYTHON "${SCRIPT_DIR}/flash_config.py" \
    "$CONFIG_FILE" \
    -o "$NVS_BIN" \
    --size 0x5000

print_success "NVS binary: $NVS_BIN"

# Warning about data loss
print_warning "This will ERASE the existing NVS partition!"
print_warning "Any runtime configuration changes will be lost."
echo ""
read -p "Continue? (y/N): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    print_info "Cancelled"
    exit 0
fi

# Flash NVS partition only
print_header "Flashing NVS Configuration"

print_info "Writing NVS to ${SERIAL_PORT}..."
$PYTHON "$ESPTOOL" \
    --chip esp32s3 \
    --port "$SERIAL_PORT" \
    write_flash \
    ${NVS_OFFSET} "$NVS_BIN"

print_success "NVS configuration updated!"

print_header "Summary"
echo "Board: ${BOARD_NAME}"
echo "Config: ${CONFIG_FILE}"
echo "Port: ${SERIAL_PORT}"
echo ""
print_success "Device will reboot with new configuration"
echo ""
print_info "To monitor serial output: pio device monitor -p ${SERIAL_PORT}"
