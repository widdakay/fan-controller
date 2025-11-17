#pragma once

#include <Arduino.h>
#include <functional>

// Log levels
enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

// Extract filename from full path (for concise logging)
#define FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

// Logging macros that automatically capture file and line information
#define LOG_DEBUG(...) Logger::log(LogLevel::DEBUG, FILENAME, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  Logger::log(LogLevel::INFO,  FILENAME, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  Logger::log(LogLevel::WARN,  FILENAME, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) Logger::log(LogLevel::ERROR, FILENAME, __LINE__, __VA_ARGS__)

// Simple logger class that wraps Serial output and optionally sends to MQTT
//
// Usage:
//   Logger::begin(115200);  // Initialize serial logging
//   Logger::setLogLevel(LogLevel::INFO);  // Set serial log level
//
//   // Enable MQTT logging after MQTT client is initialized
//   Logger::enableMqttLogging(true);
//   Logger::setMqttLogLevel(LogLevel::WARN);  // Only send WARN+ to MQTT
//   Logger::setMqttLogTopic("device/logs");
//   Logger::setMqttCallback([](const char* topic, const String& payload) {
//       return mqttClient.publish(topic, payload);
//   });
//
//   LOG_INFO("System initialized successfully");
//   LOG_ERROR("Sensor failed: %s", errorMsg);
//   LOG_DEBUG("Detailed sensor data: %d", value);
class Logger {
public:
    // Initialize the logger (call Serial.begin() externally)
    static void begin(unsigned long baud = 115200) {
        Serial.begin(baud);
    }

    // Set minimum log level for serial output (messages below this level won't be printed to serial)
    static void setLogLevel(LogLevel level) {
        minLogLevel_ = level;
    }

    // Get current serial log level
    static LogLevel getLogLevel() {
        return minLogLevel_;
    }

    // Enable/disable MQTT logging
    static void enableMqttLogging(bool enable) {
        mqttLoggingEnabled_ = enable;
    }

    // Set minimum log level for MQTT output (messages below this level won't be sent to MQTT)
    static void setMqttLogLevel(LogLevel level) {
        mqttMinLogLevel_ = level;
    }

    // Get current MQTT log level
    static LogLevel getMqttLogLevel() {
        return mqttMinLogLevel_;
    }

    // Set MQTT publish callback (should be set after MQTT client is initialized)
    static void setMqttCallback(std::function<bool(const char*, const String&)> callback) {
        mqttPublishCallback_ = callback;
    }

    // Set MQTT log topic
    static void setMqttLogTopic(const String& topic) {
        mqttLogTopic_ = topic;
    }

    // Main logging method with file and line information
    template<typename... Args>
    static void log(LogLevel level, const char* file, int line, const char* format, Args... args) {
        if (level < minLogLevel_) return;

        // Format the message with file/line info
        char formattedMessage[512];
        uint32_t timestamp = millis();
        const char* levelStr = getLevelString(level);

        // Create formatted message: [timestamp][LEVEL][file:line] message
        snprintf(formattedMessage, sizeof(formattedMessage), "[%u][%s][%s:%d] ",
                timestamp, levelStr, file, line);

        // Always log to Serial
        Serial.print(formattedMessage);
        Serial.printf(format, args...);
        Serial.println();

        // Also send to MQTT if enabled
        sendToMqtt(level, file, line, format, args...);
    }

    // Legacy methods for backwards compatibility (deprecated - use LOG_* macros instead)
    template<typename... Args>
    static void debug(const char* format, Args... args) {
        log(LogLevel::DEBUG, "unknown", 0, format, args...);
    }

    template<typename... Args>
    static void info(const char* format, Args... args) {
        log(LogLevel::INFO, "unknown", 0, format, args...);
    }

    template<typename... Args>
    static void warn(const char* format, Args... args) {
        log(LogLevel::WARN, "unknown", 0, format, args...);
    }

    template<typename... Args>
    static void error(const char* format, Args... args) {
        log(LogLevel::ERROR, "unknown", 0, format, args...);
    }

private:
    static inline LogLevel minLogLevel_ = LogLevel::INFO;
    static inline LogLevel mqttMinLogLevel_ = LogLevel::WARN;  // Only send WARN and ERROR to MQTT by default
    static inline bool mqttLoggingEnabled_ = false;
    static inline std::function<bool(const char*, const String&)> mqttPublishCallback_ = nullptr;
    static inline String mqttLogTopic_ = "logs";

    template<typename... Args>
    static void sendToMqtt(LogLevel level, const char* file, int line, const char* format, Args... args) {
        if (!mqttLoggingEnabled_ || !mqttPublishCallback_ || mqttLogTopic_.isEmpty() || level < mqttMinLogLevel_) {
            return;
        }

        // Rate limiting: don't send more than one message per second to avoid flooding
        static uint32_t lastMqttLogTime = 0;
        uint32_t now = millis();
        if (now - lastMqttLogTime < 1000) {  // 1 second minimum interval
            return;
        }
        lastMqttLogTime = now;

        // Create MQTT payload with timestamp, level, file, and line
        char payload[512];
        const char* levelStr = getLevelString(level);
        char messagePart[256];
        snprintf(messagePart, sizeof(messagePart), format, args...);
        snprintf(payload, sizeof(payload), "[%u][%s][%s:%d] %s", now, levelStr, file, line, messagePart);

        // Send to MQTT (ignore result to avoid blocking)
        mqttPublishCallback_(mqttLogTopic_.c_str(), String(payload));
    }

    template<typename... Args>
    static void logFormatted(LogLevel level, const char* format, Args... args) {
        if (level < minLogLevel_) return;

        const char* levelStr = getLevelString(level);

        // Always log to Serial
        Serial.printf("[%s] ", levelStr);
        Serial.printf(format, args...);
        Serial.println();

        // Also send to MQTT if enabled
        char buffer[256];
        snprintf(buffer, sizeof(buffer), format, args...);
        sendToMqtt(level, buffer);
    }

    static void sendToMqtt(LogLevel level, const char* message) {
        if (!mqttLoggingEnabled_ || !mqttPublishCallback_ || mqttLogTopic_.isEmpty() || level < mqttMinLogLevel_) {
            return;
        }

        // Rate limiting: don't send more than one message per second to avoid flooding
        static uint32_t lastMqttLogTime = 0;
        uint32_t now = millis();
        if (now - lastMqttLogTime < 1000) {  // 1 second minimum interval
            return;
        }
        lastMqttLogTime = now;

        // Create MQTT payload with timestamp and level
        char payload[512];
        const char* levelStr = getLevelString(level);
        snprintf(payload, sizeof(payload), "[%u][%s] %s", now, levelStr, message);

        // Send to MQTT (ignore result to avoid blocking)
        mqttPublishCallback_(mqttLogTopic_.c_str(), String(payload));
    }

    static const char* getLevelString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO:  return "INFO";
            case LogLevel::WARN:  return "WARN";
            case LogLevel::ERROR: return "ERROR";
            default:              return "UNKNOWN";
        }
    }
};

// Static member is inline-initialized above