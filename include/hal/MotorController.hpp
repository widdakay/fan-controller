#pragma once
#include "app/Types.hpp"
#include "Config.hpp"
#include <Arduino.h>

namespace hal {

class MotorController {
public:
    MotorController(int pinInA, int pinInB, int pinEnA, int pinEnB, int pinPwm,
                   uint32_t pwmFreqHz, uint8_t pwmBits);

    void begin();
    void setPower(float duty);
    void setDirection(bool forward);
    void emergencyStop();
    app::MotorStatus getStatus();
    void setFromMqtt(float value);

    float getPowerLevel() const { return status_.dutyCycle; }

private:
    int pinInA_, pinInB_, pinEnA_, pinEnB_, pinPwm_;
    uint32_t pwmFreqHz_;
    uint8_t pwmBits_;
    uint8_t pwmChannel_;
    uint32_t maxDuty_;
    app::MotorStatus status_;
};

} // namespace hal
