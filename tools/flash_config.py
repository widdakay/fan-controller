#!/usr/bin/env python3
"""
ESP32 NVS Configuration Tool

This version generates a CSV and delegates binary generation to Espressif's
official nvs_partition_gen.py (via esp-idf-nvs-partition-gen Python package).
"""

import argparse
import csv
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any, Dict


DEFAULT_SIZE = 0x5000  # 20KB matches default_16MB.csv
DEFAULT_OFFSET = 0x9000
DEFAULT_PORT = "/dev/cu.usbmodem101"
NAMESPACE = "device_cfg"


def load_config(config_path: Path) -> Dict[str, Any]:
    with config_path.open() as f:
        return json.load(f)


def build_csv(config: Dict[str, Any], csv_path: Path):
    """Translate JSON config into Espressif CSV format."""
    rows = [["key", "type", "encoding", "value"]]
    rows.append([NAMESPACE, "namespace", "", ""])

    # Device name
    if "device_name" in config:
        rows.append(["deviceName", "data", "string", config["device_name"]])

    # WiFi networks
    wifi_networks = config.get("wifi_networks", [])
    wifi_count = min(len(wifi_networks), 5)
    rows.append(["wifiCount", "data", "u8", wifi_count])
    for idx, net in enumerate(wifi_networks[:5]):
        rows.append([f"wifi{idx}ssid", "data", "string", net["ssid"]])
        rows.append([f"wifi{idx}pass", "data", "string", net["password"]])

    # MQTT server/port/topics
    if "mqtt_server" in config:
        rows.append(["mqttServer", "data", "string", config["mqtt_server"]])
    rows.append(["mqttPort", "data", "u16", config.get("mqtt_port", 1883)])
    topics = config.get("mqtt_topics", {})
    if "command" in topics:
        rows.append(["mqttCmdTopic", "data", "string", topics["command"]])
    if "status" in topics:
        rows.append(["mqttStatTopic", "data", "string", topics["status"]])

    # API endpoints
    endpoints = config.get("api_endpoints", {})
    if "influxdb" in endpoints:
        rows.append(["apiInflux", "data", "string", endpoints["influxdb"]])
    if "firmware" in endpoints:
        rows.append(["apiFwUpdate", "data", "string", endpoints["firmware"]])

    # Mark initialized
    rows.append(["initialized", "data", "u8", 1])

    with csv_path.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerows(rows)


def find_generator() -> str:
    """
    Return a path to nvs_partition_gen.py provided by esp-idf-nvs-partition-gen.
    The pip package installs a shim that calls the Python module, so we can
    invoke it directly with the current interpreter.
    """
    # Prefer local copy in tools
    local = Path(__file__).with_name("nvs_partition_gen.py")
    if local.exists():
        return str(local)

    # Fallback to PATH
    from shutil import which
    gen = which("nvs_partition_gen.py")
    if gen:
        return gen

    raise FileNotFoundError(
        "nvs_partition_gen.py not found. Install with "
        "`python -m pip install esp-idf-nvs-partition-gen`."
    )


def generate_binary(csv_path: Path, output_bin: Path, size: int):
    gen_script = find_generator()
    cmd = [
        sys.executable,
        gen_script,
        "generate",
        str(csv_path),
        str(output_bin),
        str(size),
    ]
    print(f"\nGenerating NVS with official generator:\n  {' '.join(cmd)}")
    subprocess.run(cmd, check=True)


def flash_partition(bin_path: Path, port: str, offset: int):
    esptool = Path("~/.platformio/packages/tool-esptoolpy/esptool.py").expanduser()
    if not esptool.exists():
        raise FileNotFoundError(f"esptool.py not found at {esptool}")

    cmd = [
        sys.executable,
        str(esptool),
        "--chip",
        "esp32-s3",
        "--port",
        port,
        "write_flash",
        hex(offset),
        str(bin_path),
    ]
    print(f"\nFlashing NVS partition:\n  {' '.join(cmd)}")
    subprocess.run(cmd, check=True)


def parse_args():
    parser = argparse.ArgumentParser(
        description="ESP32 NVS Configuration Tool - Uses official nvs_partition_gen.py"
    )
    parser.add_argument("config", help="JSON configuration file")
    parser.add_argument(
        "-o",
        "--output",
        default="nvs_config.bin",
        help="Output binary file (default: nvs_config.bin)",
    )
    parser.add_argument(
        "--size",
        type=lambda x: int(x, 0),
        default=DEFAULT_SIZE,
        help="Partition size in bytes (default: 0x5000)",
    )
    parser.add_argument(
        "--flash",
        action="store_true",
        help="Flash partition after generation",
    )
    parser.add_argument(
        "--port",
        default=DEFAULT_PORT,
        help=f"Serial port for flashing (default: {DEFAULT_PORT})",
    )
    parser.add_argument(
        "--offset",
        type=lambda x: int(x, 0),
        default=DEFAULT_OFFSET,
        help="Flash offset for NVS partition (default: 0x9000)",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    config_path = Path(args.config)
    output_bin = Path(args.output)

    config = load_config(config_path)

    with tempfile.TemporaryDirectory() as tmpdir:
        csv_path = Path(tmpdir) / "nvs_config.csv"
        build_csv(config, csv_path)
        generate_binary(csv_path, output_bin, args.size)

    print("\n============================================================")
    print("ESP32 NVS Configuration Tool")
    print("============================================================")
    print(f"Config file: {config_path}")
    print(f"Output file: {output_bin}")
    print(f"Partition size: {args.size} bytes")
    print("============================================================")

    if args.flash:
        flash_partition(output_bin, args.port, args.offset)
        print("\n✓ Successfully flashed NVS partition!\n")


if __name__ == "__main__":
    main()
