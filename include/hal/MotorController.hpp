#pragma once
#include "app/Types.hpp"
#include "Config.hpp"
#include <Arduino.h>

namespace hal {

class MotorController {
public:
    MotorController(int pinInA, int pinInB, int pinEnA, int pinEnB, int pinPwm,
                   uint32_t pwmFreqHz, uint8_t pwmBits)
        : pinInA_(pinInA), pinInB_(pinInB), pinEnA_(pinEnA), pinEnB_(pinEnB),
          pinPwm_(pinPwm), pwmFreqHz_(pwmFreqHz), pwmBits_(pwmBits),
          pwmChannel_(0), maxDuty_((1 << pwmBits) - 1) {}

    void begin() {
        // Configure direction pins
        pinMode(pinInA_, OUTPUT);
        pinMode(pinInB_, OUTPUT);

        // Configure enable pins as inputs with pullup (read diagnostic state)
        pinMode(pinEnA_, INPUT_PULLUP);
        pinMode(pinEnB_, INPUT_PULLUP);

        // Configure PWM
        ledcSetup(pwmChannel_, pwmFreqHz_, pwmBits_);
        ledcAttachPin(pinPwm_, pwmChannel_);

        // Start with motor off, forward direction
        setDirection(true);
        setPower(0.0f);
    }

    void setPower(float duty) {
        // Clamp to 0.0-1.0
        if (duty < 0.0f) duty = 0.0f;
        if (duty > 1.0f) duty = 1.0f;

        status_.dutyCycle = duty;

        uint32_t dutyValue = static_cast<uint32_t>(duty * maxDuty_);
        ledcWrite(pwmChannel_, dutyValue);
    }

    void setDirection(bool forward) {
        // Apply deadtime if direction is changing
        if (status_.directionForward != forward) {
            // Stop motor during direction change
            setPower(0.0f);
            delay(config::MOTOR_DIRECTION_DEADTIME_MS);
        }

        status_.directionForward = forward;

        if (forward) {
            digitalWrite(pinInA_, HIGH);
            digitalWrite(pinInB_, LOW);
        } else {
            digitalWrite(pinInA_, LOW);
            digitalWrite(pinInB_, HIGH);
        }
    }

    void emergencyStop() {
        setPower(0.0f);
        // Could also pull enable pins low if configured as outputs
    }

    app::MotorStatus getStatus() {
        // Read diagnostic pins
        status_.enAEnabled = digitalRead(pinEnA_) == HIGH;
        status_.enBEnabled = digitalRead(pinEnB_) == HIGH;
        status_.fault = !status_.enAEnabled || !status_.enBEnabled;
        return status_;
    }

    // Set motor from MQTT value (0.0-1.0)
    void setFromMqtt(float value) {
        if (value < 0.0f) value = 0.0f;
        if (value > 1.0f) value = 1.0f;
        setPower(value);
    }

    float getPowerLevel() const {
        return status_.dutyCycle;
    }

private:
    int pinInA_, pinInB_, pinEnA_, pinEnB_, pinPwm_;
    uint32_t pwmFreqHz_;
    uint8_t pwmBits_;
    uint8_t pwmChannel_;
    uint32_t maxDuty_;
    app::MotorStatus status_;
};

} // namespace hal
