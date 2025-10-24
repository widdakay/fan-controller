#pragma once
#include <cmath>

namespace util {

// Steinhart-Hart thermistor temperature calculator
class Thermistor {
public:
    // Constructor with default Steinhart-Hart coefficients for generic NTC
    explicit Thermistor(
        float R0 = 10000.0f,           // Resistance at T0
        float T0 = 25.0f,              // Reference temperature (°C)
        float beta = 3950.0f,          // Beta coefficient
        float seriesR = 10000.0f)      // Series resistor value
    : R0_(R0), T0_K_(T0 + 273.15f), beta_(beta), seriesR_(seriesR)
    {
        // Simplified Steinhart-Hart using Beta parameter
        invT0_ = 1.0f / T0_K_;
    }

    // Calculate resistance from voltage divider
    // Assumes: Vs -- seriesR -- Vpin -- thermistor -- GND
    float resistanceFromV(float Vpin, float Vs) const {
        if (Vpin <= 0.001f || Vs <= 0.001f || Vpin >= Vs) {
            return NAN;
        }

        // Vpin = Vs * Rtherm / (Rseries + Rtherm)
        // Rtherm = Rseries * Vpin / (Vs - Vpin)
        return seriesR_ * Vpin / (Vs - Vpin);
    }

    // Calculate temperature from resistance using simplified Steinhart-Hart
    float tempCFromR(float R) const {
        if (R <= 0.0f || !std::isfinite(R)) {
            return NAN;
        }

        // 1/T = 1/T0 + (1/beta) * ln(R/R0)
        float invT = invT0_ + (1.0f / beta_) * std::log(R / R0_);
        float T_K = 1.0f / invT;
        return T_K - 273.15f;  // Convert to Celsius
    }

    // Combined: voltage to temperature
    float tempCFromV(float Vpin, float Vs) const {
        float R = resistanceFromV(Vpin, Vs);
        return tempCFromR(R);
    }

    // Check if temperature is in typical valid range
    bool isValidRange(float tempC, float minC = -40.0f, float maxC = 125.0f) const {
        return std::isfinite(tempC) && tempC >= minC && tempC <= maxC;
    }

private:
    float R0_;         // Nominal resistance at T0
    float T0_K_;       // Reference temperature in Kelvin
    float beta_;       // Beta coefficient
    float seriesR_;    // Series resistor
    float invT0_;      // 1/T0 (cached)
};

} // namespace util
