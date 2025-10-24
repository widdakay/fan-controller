#pragma once
#include "Config.hpp"
#include <esp_task_wdt.h>
#include <Arduino.h>

namespace services {

class WatchdogService {
public:
    WatchdogService() = default;

    void begin(uint32_t timeoutMs = config::WATCHDOG_TIMEOUT_MS) {
        // Configure watchdog
        esp_task_wdt_init(timeoutMs / 1000, true);  // timeout in seconds, panic on timeout
        esp_task_wdt_add(NULL);  // Add current task to watchdog

        Serial.printf("Watchdog initialized with %lums timeout\n", timeoutMs);
    }

    void feed() {
        esp_task_wdt_reset();
    }

    void disable() {
        esp_task_wdt_delete(NULL);
    }

private:
};

} // namespace services
