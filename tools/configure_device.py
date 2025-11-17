#!/usr/bin/env python3
"""
ESP32 Device Configuration Tool
Configure ESP32 devices over MQTT using JSON configuration files.

Usage:
    python configure_device.py config.json
    python configure_device.py config.json --broker 10.10.1.20
    python configure_device.py config.json --device TestBoard3
"""

import json
import sys
import time
import argparse
from typing import Dict, Any, Optional
import paho.mqtt.client as mqtt


class DeviceConfigurator:
    """Configure ESP32 devices over MQTT"""

    def __init__(self, broker: str, port: int = 1883, timeout: int = 5):
        """
        Initialize the configurator

        Args:
            broker: MQTT broker address
            port: MQTT broker port (default: 1883)
            timeout: Response timeout in seconds (default: 5)
        """
        self.broker = broker
        self.port = port
        self.timeout = timeout
        self.client = mqtt.Client()
        self.response_received = False
        self.last_response = None

        # Set up callbacks
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message

    def _on_connect(self, client, userdata, flags, rc):
        """Callback for when the client connects to the broker"""
        if rc == 0:
            print(f"✓ Connected to MQTT broker at {self.broker}:{self.port}")
        else:
            print(f"✗ Failed to connect to MQTT broker (code: {rc})")

    def _on_message(self, client, userdata, msg):
        """Callback for when a message is received"""
        self.response_received = True
        self.last_response = msg.payload.decode('utf-8')
        print(f"  Response: {self.last_response}")

    def connect(self) -> bool:
        """Connect to MQTT broker"""
        try:
            self.client.connect(self.broker, self.port, 60)
            self.client.loop_start()
            time.sleep(1)  # Give it time to connect
            return True
        except Exception as e:
            print(f"✗ Error connecting to broker: {e}")
            return False

    def disconnect(self):
        """Disconnect from MQTT broker"""
        self.client.loop_stop()
        self.client.disconnect()
        print("\n✓ Disconnected from MQTT broker")

    def send_command(self, config_topic: str, command: Dict[str, Any],
                    status_topic: Optional[str] = None) -> bool:
        """
        Send a configuration command and wait for response

        Args:
            config_topic: Topic to send command to (e.g., "device/fan1/config")
            command: Command dictionary
            status_topic: Optional status topic to subscribe to

        Returns:
            True if command was sent successfully
        """
        # Subscribe to status topic if provided
        if status_topic:
            self.client.subscribe(status_topic)

        # Reset response flag
        self.response_received = False
        self.last_response = None

        # Send command
        cmd_json = json.dumps(command)
        result = self.client.publish(config_topic, cmd_json)

        if result.rc != mqtt.MQTT_ERR_SUCCESS:
            print(f"  ✗ Failed to send command")
            return False

        # Wait for response if status topic provided
        if status_topic:
            start_time = time.time()
            while not self.response_received and (time.time() - start_time) < self.timeout:
                time.sleep(0.1)

            if not self.response_received:
                print(f"  ⚠ No response received (timeout)")
                return False

        return True

    def configure_device(self, device_name: str, config: Dict[str, Any]) -> bool:
        """
        Configure a device using the provided configuration

        Args:
            device_name: Name/topic prefix of the device
            config: Configuration dictionary

        Returns:
            True if all commands succeeded
        """
        # Determine topics from config or device name
        base_topic = config.get("base_topic", f"{device_name}/fan1")
        config_topic = f"{base_topic}/config"
        status_topic = f"{base_topic}/config/status"

        print(f"\n{'='*60}")
        print(f"Configuring device: {device_name}")
        print(f"Config topic: {config_topic}")
        print(f"Status topic: {status_topic}")
        print(f"{'='*60}\n")

        success = True

        # 1. Set Device Name
        if "device_name" in config:
            print(f"[1/6] Setting device name to '{config['device_name']}'...")
            cmd = {
                "cmd": "set_device_name",
                "name": config["device_name"]
            }
            if not self.send_command(config_topic, cmd, status_topic):
                success = False
            time.sleep(0.5)

        # 2. Set WiFi Credentials
        if "wifi_networks" in config:
            for idx, wifi in enumerate(config["wifi_networks"]):
                print(f"[2/6] Setting WiFi network {idx}: '{wifi['ssid']}'...")
                cmd = {
                    "cmd": "set_wifi",
                    "index": idx,
                    "ssid": wifi["ssid"],
                    "password": wifi["password"]
                }
                if not self.send_command(config_topic, cmd, status_topic):
                    success = False
                time.sleep(0.5)

        # 3. Set MQTT Server
        if "mqtt_server" in config:
            print(f"[3/6] Setting MQTT server to '{config['mqtt_server']}'...")
            cmd = {
                "cmd": "set_mqtt_server",
                "server": config["mqtt_server"],
                "port": config.get("mqtt_port", 1883)
            }
            if not self.send_command(config_topic, cmd, status_topic):
                success = False
            time.sleep(0.5)

        # 4. Set MQTT Topics
        if "mqtt_topics" in config:
            topics = config["mqtt_topics"]
            print(f"[4/6] Setting MQTT topics...")
            cmd = {
                "cmd": "set_mqtt_topics",
                "command": topics["command"],
                "status": topics["status"]
            }
            if not self.send_command(config_topic, cmd, status_topic):
                success = False
            time.sleep(0.5)

        # 5. Set API Endpoints
        if "api_endpoints" in config:
            endpoints = config["api_endpoints"]
            print(f"[5/6] Setting API endpoints...")
            cmd = {
                "cmd": "set_api_endpoints",
                "influxdb": endpoints["influxdb"],
                "firmware": endpoints["firmware"]
            }
            if not self.send_command(config_topic, cmd, status_topic):
                success = False
            time.sleep(0.5)

        # 6. Print Config (verify)
        print(f"[6/6] Requesting configuration printout...")
        cmd = {"cmd": "print_config"}
        if not self.send_command(config_topic, cmd, status_topic):
            success = False

        return success


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


def main():
    parser = argparse.ArgumentParser(
        description="Configure ESP32 devices over MQTT",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  Configure a device using config.json:
    python configure_device.py config.json

  Specify MQTT broker:
    python configure_device.py config.json --broker 10.10.1.20

  Override device name:
    python configure_device.py config.json --device MyDevice

  Print current configuration:
    python configure_device.py --print-config --device TestBoard3 --broker 10.10.1.20
        """
    )

    parser.add_argument('config_file', nargs='?', help='JSON configuration file')
    parser.add_argument('--broker', default='10.10.1.20',
                       help='MQTT broker address (default: 10.10.1.20)')
    parser.add_argument('--port', type=int, default=1883,
                       help='MQTT broker port (default: 1883)')
    parser.add_argument('--device', help='Device name/topic prefix (overrides config file)')
    parser.add_argument('--timeout', type=int, default=5,
                       help='Response timeout in seconds (default: 5)')
    parser.add_argument('--print-config', action='store_true',
                       help='Only print device configuration (requires --device)')
    parser.add_argument('--reset-config', action='store_true',
                       help='Reset device to defaults (requires --device)')

    args = parser.parse_args()

    # Validate arguments
    if not args.print_config and not args.reset_config and not args.config_file:
        parser.error("config_file is required unless --print-config or --reset-config is specified")

    if (args.print_config or args.reset_config) and not args.device:
        parser.error("--device is required when using --print-config or --reset-config")

    # Create configurator
    configurator = DeviceConfigurator(args.broker, args.port, args.timeout)

    # Connect to broker
    if not configurator.connect():
        sys.exit(1)

    try:
        # Handle special commands
        if args.print_config:
            device_name = args.device
            base_topic = f"{device_name}/fan1"
            config_topic = f"{base_topic}/config"
            status_topic = f"{base_topic}/config/status"

            print(f"\nRequesting configuration from {device_name}...")
            configurator.send_command(config_topic, {"cmd": "print_config"}, status_topic)
            print("\nCheck device serial output for configuration details")

        elif args.reset_config:
            device_name = args.device
            base_topic = f"{device_name}/fan1"
            config_topic = f"{base_topic}/config"
            status_topic = f"{base_topic}/config/status"

            print(f"\n⚠ WARNING: This will reset {device_name} to factory defaults!")
            response = input("Are you sure? (yes/no): ")
            if response.lower() == 'yes':
                configurator.send_command(config_topic, {"cmd": "reset_config"}, status_topic)
                print("\n✓ Reset command sent. Device will require restart.")
            else:
                print("Cancelled.")

        else:
            # Load and apply configuration
            config = load_config(args.config_file)

            # Override device name if provided
            if args.device:
                device_name = args.device
            elif "device_name" in config:
                # Use first part of device name as topic prefix
                device_name = config.get("topic_prefix", config["device_name"])
            else:
                print("✗ No device name specified (use --device or include in config)")
                sys.exit(1)

            # Configure device
            success = configurator.configure_device(device_name, config)

            if success:
                print(f"\n{'='*60}")
                print("✓ Configuration completed successfully!")
                print("⚠ Most changes require a device restart to take effect")
                print(f"{'='*60}\n")
            else:
                print(f"\n{'='*60}")
                print("⚠ Configuration completed with some errors")
                print("  Check the responses above for details")
                print(f"{'='*60}\n")

    finally:
        configurator.disconnect()


if __name__ == "__main__":
    main()
