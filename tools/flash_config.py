#!/usr/bin/env python3
"""
ESP32 NVS Configuration Tool
Unified tool to configure ESP32 devices by flashing NVS partitions.

This script reads JSON configuration and directly creates and flashes
NVS partition binaries without requiring ESP-IDF tools.

Usage:
    python flash_config.py config.json                    # Generate only
    python flash_config.py config.json --flash            # Generate and flash
    python flash_config.py config.json --port /dev/ttyUSB0 --flash
"""

import json
import struct
import sys
import os
import argparse
import subprocess
from typing import Dict, Any, List, Tuple
from enum import IntEnum


class NVSType(IntEnum):
    """NVS data types"""
    U8 = 0x01
    I8 = 0x11
    U16 = 0x02
    I16 = 0x12
    U32 = 0x04
    I32 = 0x14
    U64 = 0x08
    I64 = 0x18
    STR = 0x21
    BLOB = 0x42
    BLOB_DATA = 0x41
    BLOB_IDX = 0x48


class NVSEntry:
    """Represents a single NVS entry"""

    def __init__(self, namespace: str, key: str, data_type: NVSType, value: Any):
        self.namespace = namespace
        self.key = key
        self.data_type = data_type
        self.value = value

        if len(key) > 15:
            raise ValueError(f"Key '{key}' exceeds 15 character limit")

    def encode(self) -> bytes:
        """Encode entry to 32-byte NVS format"""
        entry = bytearray(32)

        # Namespace index (0xFF for active)
        entry[0] = 0xFF

        # Type
        entry[1] = self.data_type

        # Span (always 1 for non-blob)
        entry[2] = 0x01

        # Reserved
        entry[3] = 0xFF

        # CRC32 placeholder (calculated later)
        entry[4:8] = b'\x00\x00\x00\x00'

        # Key (null-terminated, max 15 chars + null)
        key_bytes = self.key.encode('utf-8')[:15]
        entry[8:8+len(key_bytes)] = key_bytes
        entry[8+len(key_bytes)] = 0  # Null terminator

        # Data (8 bytes)
        data_offset = 24
        if self.data_type == NVSType.U8:
            struct.pack_into('<B', entry, data_offset, int(self.value))
        elif self.data_type == NVSType.U16:
            struct.pack_into('<H', entry, data_offset, int(self.value))
        elif self.data_type == NVSType.U32:
            struct.pack_into('<I', entry, data_offset, int(self.value))
        elif self.data_type == NVSType.STR:
            # String is stored as blob, data contains size and index
            str_data = str(self.value).encode('utf-8')
            struct.pack_into('<H', entry, data_offset, len(str_data) + 1)  # +1 for null
            # For strings, we'll handle blob storage separately
            self.blob_data = str_data + b'\x00'

        # Calculate CRC32
        crc = self._crc32(entry[4:])
        struct.pack_into('<I', entry, 4, crc)

        return bytes(entry)

    @staticmethod
    def _crc32(data: bytes) -> int:
        """Calculate CRC32"""
        import zlib
        return zlib.crc32(data) & 0xFFFFFFFF


class NVSPartition:
    """NVS Partition manager"""

    NAMESPACE = "device_cfg"
    PAGE_SIZE = 4096
    ENTRY_SIZE = 32

    def __init__(self, size: int = 0x5000):
        """
        Initialize NVS partition

        Args:
            size: Partition size in bytes (default 20KB)
        """
        if size % self.PAGE_SIZE != 0:
            raise ValueError("Size must be multiple of 4096")

        self.size = size
        self.num_pages = size // self.PAGE_SIZE
        self.entries = []
        self.string_data = {}  # key -> string data

    def add_u8(self, key: str, value: int):
        """Add unsigned 8-bit integer"""
        self.entries.append(NVSEntry(self.NAMESPACE, key, NVSType.U8, value))

    def add_u16(self, key: str, value: int):
        """Add unsigned 16-bit integer"""
        self.entries.append(NVSEntry(self.NAMESPACE, key, NVSType.U16, value))

    def add_u32(self, key: str, value: int):
        """Add unsigned 32-bit integer"""
        self.entries.append(NVSEntry(self.NAMESPACE, key, NVSType.U32, value))

    def add_string(self, key: str, value: str):
        """Add string value"""
        if len(value) > 4000:  # Conservative limit
            raise ValueError(f"String '{key}' too long (max ~4000 chars)")
        self.string_data[key] = value
        self.entries.append(NVSEntry(self.NAMESPACE, key, NVSType.STR, value))

    def from_config(self, config: Dict[str, Any]):
        """
        Populate partition from configuration dictionary

        Args:
            config: Configuration dictionary from JSON
        """
        # Device name
        if "device_name" in config:
            self.add_string("deviceName", config["device_name"])

        # WiFi credentials
        if "wifi_networks" in config:
            wifi_count = min(len(config["wifi_networks"]), 5)
            self.add_u8("wifiCount", wifi_count)

            for idx, wifi in enumerate(config["wifi_networks"][:5]):
                self.add_string(f"wifi{idx}ssid", wifi["ssid"])
                self.add_string(f"wifi{idx}pass", wifi["password"])

        # MQTT configuration
        if "mqtt_server" in config:
            self.add_string("mqttServer", config["mqtt_server"])

        if "mqtt_port" in config:
            self.add_u16("mqttPort", config["mqtt_port"])
        else:
            self.add_u16("mqttPort", 1883)

        if "mqtt_topics" in config:
            topics = config["mqtt_topics"]
            self.add_string("mqttCmdTopic", topics["command"])
            self.add_string("mqttStatTopic", topics["status"])

        # API endpoints
        if "api_endpoints" in config:
            endpoints = config["api_endpoints"]
            self.add_string("apiInflux", endpoints["influxdb"])
            self.add_string("apiFwUpdate", endpoints["firmware"])

        # Mark as initialized
        self.add_u8("initialized", 1)

    def generate_binary(self) -> bytes:
        """
        Generate NVS partition binary

        Returns:
            Binary data for NVS partition
        """
        # Create binary buffer
        binary = bytearray(self.size)

        # Page 0: Header + Entries
        page_offset = 0

        # Write page header
        self._write_page_header(binary, page_offset, 0, seq_num=0)

        # Write namespace entry first
        entry_offset = page_offset + 32  # After page header
        ns_entry = self._create_namespace_entry()
        binary[entry_offset:entry_offset+32] = ns_entry
        entry_offset += 32

        # Write data entries
        for entry in self.entries:
            if entry_offset + 32 > page_offset + self.PAGE_SIZE:
                # Move to next page
                page_offset += self.PAGE_SIZE
                if page_offset >= self.size:
                    raise ValueError("Partition too small for all entries")
                self._write_page_header(binary, page_offset, page_offset // self.PAGE_SIZE, seq_num=0)
                entry_offset = page_offset + 32

            if entry.data_type == NVSType.STR:
                # Write string as simple blob
                string_bytes = self.string_data[entry.key].encode('utf-8') + b'\x00'

                # Create blob entry
                blob_entry = bytearray(32)
                blob_entry[0] = 0xFF  # Namespace
                blob_entry[1] = NVSType.STR  # Type
                blob_entry[2] = 0x01  # Span
                blob_entry[3] = 0xFF  # Reserved

                # Key
                key_bytes = entry.key.encode('utf-8')[:15]
                blob_entry[8:8+len(key_bytes)] = key_bytes
                blob_entry[8+len(key_bytes)] = 0

                # Size in data field
                struct.pack_into('<H', blob_entry, 24, len(string_bytes))

                # Store string data in next entries
                # For simplicity, store inline if small enough
                if len(string_bytes) <= 8:
                    blob_entry[24:24+len(string_bytes)] = string_bytes
                else:
                    # Store size and chunk index
                    struct.pack_into('<H', blob_entry, 24, len(string_bytes))
                    struct.pack_into('<H', blob_entry, 26, 0)  # Chunk index

                # CRC
                crc = self._crc32(blob_entry[4:])
                struct.pack_into('<I', blob_entry, 4, crc)

                binary[entry_offset:entry_offset+32] = blob_entry
                entry_offset += 32

                # Write string data if needed
                if len(string_bytes) > 8:
                    # Write blob data entries
                    chunk_offset = 0
                    chunk_index = 0
                    while chunk_offset < len(string_bytes):
                        chunk_size = min(32, len(string_bytes) - chunk_offset)
                        chunk_data = string_bytes[chunk_offset:chunk_offset+chunk_size]

                        data_entry = bytearray(32)
                        data_entry[0] = 0xFF
                        data_entry[1] = NVSType.BLOB_DATA
                        data_entry[2] = 0x01
                        data_entry[3] = 0xFF

                        # Copy chunk data
                        data_entry[8:8+len(chunk_data)] = chunk_data

                        # CRC
                        crc = self._crc32(data_entry[4:])
                        struct.pack_into('<I', data_entry, 4, crc)

                        binary[entry_offset:entry_offset+32] = data_entry
                        entry_offset += 32
                        chunk_offset += chunk_size
                        chunk_index += 1
            else:
                # Simple numeric entry
                encoded = entry.encode()
                binary[entry_offset:entry_offset+32] = encoded
                entry_offset += 32

        # Fill rest with 0xFF (erased flash)
        for i in range(entry_offset, self.size):
            binary[i] = 0xFF

        return bytes(binary)

    def _write_page_header(self, binary: bytearray, offset: int, page_num: int, seq_num: int):
        """Write NVS page header"""
        header = bytearray(32)

        # State (0xFFFFFFFE = active)
        struct.pack_into('<I', header, 0, 0xFFFFFFFE)

        # Sequence number
        struct.pack_into('<I', header, 4, seq_num)

        # Version (0xFF = V1)
        header[8] = 0xFF

        # Fill rest with 0xFF
        for i in range(9, 32):
            header[i] = 0xFF

        # CRC (of state + seq_num)
        crc = self._crc32(header[:8])
        struct.pack_into('<I', header, 28, crc)

        binary[offset:offset+32] = header

    def _create_namespace_entry(self) -> bytes:
        """Create namespace entry"""
        entry = bytearray(32)

        # Namespace index 0
        entry[0] = 0x00

        # Type: namespace
        entry[1] = 0x00

        # Span
        entry[2] = 0x01

        # Reserved
        entry[3] = 0xFF

        # Namespace name
        ns_bytes = self.NAMESPACE.encode('utf-8')[:15]
        entry[8:8+len(ns_bytes)] = ns_bytes
        entry[8+len(ns_bytes)] = 0

        # CRC
        crc = self._crc32(entry[4:])
        struct.pack_into('<I', entry, 4, crc)

        return bytes(entry)

    @staticmethod
    def _crc32(data: bytes) -> int:
        """Calculate CRC32"""
        import zlib
        return zlib.crc32(data) & 0xFFFFFFFF


def load_config(config_file: str) -> Dict[str, Any]:
    """Load configuration from JSON file"""
    try:
        with open(config_file, 'r') as f:
            return json.load(f)
    except FileNotFoundError:
        print(f"✗ Configuration file not found: {config_file}")
        sys.exit(1)
    except json.JSONDecodeError as e:
        print(f"✗ Invalid JSON in configuration file: {e}")
        sys.exit(1)


def flash_partition(binary_file: str, port: str, offset: str = "0x9000") -> bool:
    """
    Flash NVS partition to ESP32

    Args:
        binary_file: Binary partition file to flash
        port: Serial port
        offset: Flash offset for NVS partition (default: 0x9000)

    Returns:
        True if successful
    """
    # Try to find esptool
    esptool_paths = [
        os.path.expanduser("~/.platformio/packages/tool-esptoolpy/esptool.py"),
        "esptool.py"
    ]

    esptool = None
    for path in esptool_paths:
        if os.path.exists(path):
            esptool = path
            break

    if not esptool:
        esptool = "esptool.py"  # Try from PATH

    try:
        # Use esptool directly (it's a Python script)
        cmd = [
            esptool,
            "--chip", "esp32s3",
            "--port", port,
            "--baud", "460800",
            "write_flash",
            offset,
            binary_file
        ]

        print(f"\n{'='*60}")
        print(f"Flashing NVS partition...")
        print(f"  Port: {port}")
        print(f"  Offset: {offset}")
        print(f"  File: {binary_file}")
        print(f"{'='*60}\n")

        result = subprocess.run(cmd, check=False)

        if result.returncode == 0:
            print(f"\n{'='*60}")
            print("✓ Successfully flashed NVS partition!")
            print(f"{'='*60}\n")
            return True
        else:
            print(f"\n✗ Flash failed with exit code {result.returncode}")
            return False

    except FileNotFoundError:
        print("\n✗ Error: esptool.py not found")
        print("\nPlease install esptool:")
        print("  pip install esptool")
        print("\nOr use PlatformIO's esptool:")
        print(f"  ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32s3 --port {port} write_flash {offset} {binary_file}")
        return False

    except Exception as e:
        print(f"\n✗ Error: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(
        description="ESP32 NVS Configuration Tool - Generate and flash device configuration",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  Generate NVS binary:
    python flash_config.py config.json

  Generate and flash:
    python flash_config.py config.json --flash

  Specify custom port:
    python flash_config.py config.json --port /dev/ttyUSB0 --flash

  Custom output file:
    python flash_config.py config.json -o my_config.bin
        """
    )

    parser.add_argument('config', help='JSON configuration file')
    parser.add_argument('-o', '--output', default='nvs_config.bin',
                       help='Output binary file (default: nvs_config.bin)')
    parser.add_argument('--size', default='0x5000',
                       help='Partition size in bytes (default: 0x5000 = 20KB)')
    parser.add_argument('--flash', action='store_true',
                       help='Flash partition after generation')
    parser.add_argument('--port', default='/dev/cu.usbmodem101',
                       help='Serial port for flashing (default: /dev/cu.usbmodem101)')
    parser.add_argument('--offset', default='0x9000',
                       help='Flash offset for NVS partition (default: 0x9000)')

    args = parser.parse_args()

    # Load configuration
    config = load_config(args.config)

    # Parse size
    try:
        size = int(args.size, 0)
    except ValueError:
        print(f"✗ Invalid size: {args.size}")
        sys.exit(1)

    print(f"\n{'='*60}")
    print("ESP32 NVS Configuration Tool")
    print(f"{'='*60}")
    print(f"Config file: {args.config}")
    print(f"Device name: {config.get('device_name', 'N/A')}")
    print(f"WiFi networks: {len(config.get('wifi_networks', []))}")
    print(f"Output file: {args.output}")
    print(f"Size: {size} bytes ({size // 1024}KB)")
    print(f"{'='*60}\n")

    # Create NVS partition
    try:
        nvs = NVSPartition(size)
        nvs.from_config(config)

        print(f"Generating NVS partition with {len(nvs.entries)} entries...")
        binary = nvs.generate_binary()

        # Write to file
        with open(args.output, 'wb') as f:
            f.write(binary)

        print(f"✓ Created NVS binary: {args.output}")
        print(f"  Size: {len(binary)} bytes")

        # Flash if requested
        if args.flash:
            success = flash_partition(args.output, args.port, args.offset)
            if not success:
                sys.exit(1)
        else:
            print(f"\nTo flash manually:")
            print(f"  python3 flash_config.py {args.config} --flash --port {args.port}")
            print(f"\nOr use esptool directly:")
            print(f"  esptool.py --chip esp32s3 --port {args.port} write_flash {args.offset} {args.output}")

    except Exception as e:
        print(f"\n✗ Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
