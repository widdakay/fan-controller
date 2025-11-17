#!/bin/bash

# ESP32 MQTT Logger Setup Script
# Installs required dependencies for monitoring ESP32 device logs via MQTT

echo "🚀 Setting up ESP32 MQTT Logger..."

# Check if Python 3 is installed
if ! command -v python3 &> /dev/null; then
    echo "❌ Python 3 is not installed. Please install Python 3 first."
    exit 1
fi

echo "📦 Installing paho-mqtt library..."
pip3 install paho-mqtt

if [ $? -eq 0 ]; then
    echo "✅ Installation successful!"
    echo ""
    echo "🎯 Usage:"
    echo "  python3 mqtt_logger.py                    # Connect to localhost:1883, topic: testboard3/logs"
    echo "  python3 mqtt_logger.py 192.168.1.100      # Connect to specific broker"
    echo "  python3 mqtt_logger.py 192.168.1.100 1883 # Specify broker and port"
    echo "  python3 mqtt_logger.py broker 1883 topic  # Specify all parameters"
    echo ""
    echo "📡 Make sure your ESP32 device is connected to the same MQTT broker!"
else
    echo "❌ Installation failed. Please try: pip3 install paho-mqtt"
    exit 1
fi