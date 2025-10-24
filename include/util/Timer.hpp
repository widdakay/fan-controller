#pragma once
#include <Arduino.h>
#include <cstdint>

namespace util {

// Non-blocking interval timer
class Timer {
public:
    explicit Timer(uint32_t intervalMs = 0)
        : intervalMs_(intervalMs), lastTriggerMs_(0) {}

    // Set interval in milliseconds
    void setInterval(uint32_t intervalMs) {
        intervalMs_ = intervalMs;
    }

    // Check if timer has elapsed (automatically resets)
    bool check() {
        uint32_t now = millis();
        if (now - lastTriggerMs_ >= intervalMs_) {
            lastTriggerMs_ = now;
            return true;
        }
        return false;
    }

    // Reset timer to current time
    void reset() {
        lastTriggerMs_ = millis();
    }

    // Check elapsed time without resetting
    uint32_t elapsed() const {
        return millis() - lastTriggerMs_;
    }

    // Check if elapsed without resetting
    bool hasElapsed() const {
        return millis() - lastTriggerMs_ >= intervalMs_;
    }

    // Get remaining time until next trigger
    uint32_t remaining() const {
        uint32_t e = elapsed();
        return (e >= intervalMs_) ? 0 : (intervalMs_ - e);
    }

private:
    uint32_t intervalMs_;
    uint32_t lastTriggerMs_;
};

// One-shot timer
class OneShotTimer {
public:
    OneShotTimer() : targetMs_(0), active_(false) {}

    void start(uint32_t durationMs) {
        targetMs_ = millis() + durationMs;
        active_ = true;
    }

    void stop() {
        active_ = false;
    }

    bool isActive() const {
        return active_;
    }

    bool hasExpired() {
        if (!active_) return false;
        if (millis() >= targetMs_) {
            active_ = false;
            return true;
        }
        return false;
    }

private:
    uint32_t targetMs_;
    bool active_;
};

} // namespace util
