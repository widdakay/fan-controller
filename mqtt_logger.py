#!/usr/bin/env python3
"""
MQTT Logger Subscriber for ESP32 Device Logs

This script subscribes to the MQTT topic for device logs and displays them in real-time.
It helps monitor the ESP32 device's logging output remotely.

Usage:
    python mqtt_logger.py [mqtt_broker] [port] [topic]

Default values:
    - MQTT Broker: localhost (change to your MQTT server IP)
    - Port: 1883 (standard MQTT port)
    - Topic: testboard3/logs (device logs topic)
"""

import sys
import time
from datetime import datetime
import paho.mqtt.client as mqtt

# Default MQTT settings
DEFAULT_BROKER = "localhost"  # Change this to your MQTT broker IP
DEFAULT_PORT = 1883
DEFAULT_TOPIC = "testboard3/logs"

class MQTTLogger:
    def __init__(self, broker=DEFAULT_BROKER, port=DEFAULT_PORT, topic=DEFAULT_TOPIC):
        self.broker = broker
        self.port = port
        self.topic = topic
        self.client = None
        self.connected = False

    def on_connect(self, client, userdata, flags, rc):
        """Called when the client connects to the MQTT broker"""
        if rc == 0:
            self.connected = True
            print(f"✅ Connected to MQTT broker {self.broker}:{self.port}")
            print(f"📡 Subscribed to topic: {self.topic}")
            print("=" * 80)
            client.subscribe(self.topic)
        else:
            print(f"❌ Failed to connect to MQTT broker. Return code: {rc}")

    def on_disconnect(self, client, userdata, rc):
        """Called when the client disconnects from the MQTT broker"""
        self.connected = False
        if rc != 0:
            print(f"⚠️  Unexpected disconnection from MQTT broker. Return code: {rc}")
        else:
            print("🔌 Disconnected from MQTT broker")

    def on_message(self, client, userdata, msg):
        """Called when a message is received on the subscribed topic"""
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        try:
            # Try to decode as UTF-8
            payload = msg.payload.decode('utf-8')
            print(f"[{timestamp}] 📨 {msg.topic}: {payload}")
        except UnicodeDecodeError:
            # If it's not valid UTF-8, show as hex
            payload_hex = msg.payload.hex()
            print(f"[{timestamp}] 📨 {msg.topic}: [BINARY] {payload_hex}")

    def connect(self):
        """Connect to the MQTT broker"""
        self.client = mqtt.Client()
        self.client.on_connect = self.on_connect
        self.client.on_disconnect = self.on_disconnect
        self.client.on_message = self.on_message

        try:
            print(f"🔌 Connecting to MQTT broker {self.broker}:{self.port}...")
            self.client.connect(self.broker, self.port, 60)
            return True
        except Exception as e:
            print(f"❌ Failed to connect to MQTT broker: {e}")
            return False

    def run(self):
        """Main loop to keep the MQTT client running"""
        if not self.connect():
            return

        print("\n🎧 Listening for ESP32 device logs...")
        print("Press Ctrl+C to stop\n")

        try:
            # Start the MQTT client loop
            self.client.loop_forever()
        except KeyboardInterrupt:
            print("\n\n🛑 Stopping MQTT logger...")
            if self.client:
                self.client.disconnect()
        except Exception as e:
            print(f"\n❌ Error in MQTT loop: {e}")

def main():
    # Parse command line arguments
    broker = DEFAULT_BROKER
    port = DEFAULT_PORT
    topic = DEFAULT_TOPIC

    if len(sys.argv) > 1:
        broker = sys.argv[1]
    if len(sys.argv) > 2:
        try:
            port = int(sys.argv[2])
        except ValueError:
            print(f"❌ Invalid port number: {sys.argv[2]}")
            return
    if len(sys.argv) > 3:
        topic = sys.argv[3]

    print("🚀 ESP32 MQTT Logger Subscriber")
    print(f"📍 Broker: {broker}:{port}")
    print(f"📡 Topic: {topic}")
    print("-" * 40)

    logger = MQTTLogger(broker, port, topic)
    logger.run()

if __name__ == "__main__":
    main()