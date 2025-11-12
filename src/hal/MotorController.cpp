#include "hal/MotorController.hpp"

namespace hal {

MotorController::MotorController(int pinInA, int pinInB, int pinEnA, int pinEnB, int pinPwm,
                                 uint32_t pwmFreqHz, uint8_t pwmBits)
    : pinInA_(pinInA), pinInB_(pinInB), pinEnA_(pinEnA), pinEnB_(pinEnB),
      pinPwm_(pinPwm), pwmFreqHz_(pwmFreqHz), pwmBits_(pwmBits),
      pwmChannel_(0), maxDuty_((1 << pwmBits) - 1) {}

void MotorController::begin() {
    // Configure direction pins
    pinMode(pinInA_, OUTPUT);
    pinMode(pinInB_, OUTPUT);

    // Configure enable pins as inputs with pullup (read diagnostic state)
    pinMode(pinEnA_, INPUT_PULLUP);
    pinMode(pinEnB_, INPUT_PULLUP);

    // Configure PWM
    ledcSetup(pwmChannel_, pwmFreqHz_, pwmBits_);
    ledcAttachPin(pinPwm_, pwmChannel_);

    // Start with motor off, reversed direction
    setDirection(false);
    setPower(0.0f);
}

void MotorController::setPower(float duty) {
    // Clamp to 0.0-1.0
    if (duty < 0.0f) duty = 0.0f;
    if (duty > 1.0f) duty = 1.0f;

    status_.dutyCycle = duty;

    uint32_t dutyValue = static_cast<uint32_t>(duty * maxDuty_);
    ledcWrite(pwmChannel_, dutyValue);
}

void MotorController::setDirection(bool forward) {
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

void MotorController::emergencyStop() {
    setPower(0.0f);
    // Could also pull enable pins low if configured as outputs
}

app::MotorStatus MotorController::getStatus() {
    // Read diagnostic pins
    status_.enAEnabled = digitalRead(pinEnA_) == HIGH;
    status_.enBEnabled = digitalRead(pinEnB_) == HIGH;
    status_.fault = !status_.enAEnabled || !status_.enBEnabled;
    return status_;
}

void MotorController::setFromMqtt(float value) {
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;
    setPower(value);
}

} // namespace hal
