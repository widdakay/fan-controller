#pragma once
#include "Config.hpp"
#include "util/Timer.hpp"
#include <Arduino.h>

namespace hal {

class LedController {
public:
    enum class Led {
        Green,   // Heartbeat
        Orange,  // OTA update
        Red,     // Error
        Blue     // Motor on
    };

    LedController() {
        pinMode(config::PIN_LED_GREEN, OUTPUT);
        pinMode(config::PIN_LED_ORANGE, OUTPUT);
        pinMode(config::PIN_LED_RED, OUTPUT);
        pinMode(config::PIN_LED_BLUE, OUTPUT);

        allOff();
    }

    void set(Led led, bool state) {
        digitalWrite(pinForLed(led), state ? HIGH : LOW);
    }

    void toggle(Led led) {
        int pin = pinForLed(led);
        digitalWrite(pin, !digitalRead(pin));
    }

    void flash(Led led, uint32_t durationMs) {
        set(led, true);
        flashTimer_.start(durationMs);
        flashLed_ = led;
    }

    void pulse(Led led) {
        flash(led, 50);  // Quick pulse
    }

    // Call from main loop to handle timed flashes
    void update() {
        if (flashTimer_.hasExpired()) {
            set(flashLed_, false);
        }
    }

    void allOff() {
        set(Led::Green, false);
        set(Led::Orange, false);
        set(Led::Red, false);
        set(Led::Blue, false);
    }

    // Status indication methods
    void heartbeat() {
        flash(Led::Green, config::LED_HEARTBEAT_MS);
    }

    void errorFlash() {
        flash(Led::Red, config::LED_ERROR_FLASH_MS);
    }

    void setMotorStatus(bool motorOn) {
        set(Led::Blue, motorOn);
    }

    void setOtaStatus(bool otaActive) {
        set(Led::Orange, otaActive);
    }

private:
    int pinForLed(Led led) const {
        switch (led) {
            case Led::Green:  return config::PIN_LED_GREEN;
            case Led::Orange: return config::PIN_LED_ORANGE;
            case Led::Red:    return config::PIN_LED_RED;
            case Led::Blue:   return config::PIN_LED_BLUE;
            default:          return config::PIN_LED_GREEN;
        }
    }

    util::OneShotTimer flashTimer_;
    Led flashLed_ = Led::Green;
};

} // namespace hal
