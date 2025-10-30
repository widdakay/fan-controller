#pragma once
#include <cmath>

namespace util {

// Steinhart-Hart thermistor temperature calculator using full A, B, C coefficients
class ThermistorSH {
public:
    // Murata 10k fit (25/50/85 °C)
    ThermistorSH(float Rseries = 10'000.0f,
                 float A = 8.68830973e-04f,
                 float B = 2.54720308e-04f,
                 float C = 1.78064471e-07f)
    : R_series_{Rseries}, A_{A}, B_{B}, C_{C} {}

    [[nodiscard]] float resistanceFromV(float vout, float vs) const {
        constexpr float eps = 1e-6f;
        if (!std::isfinite(vout) || !std::isfinite(vs) || vout <= eps || vout >= (vs - eps)) return NAN;
        // Divider: Vout = Vs * (R_series / (R_series + R_ntc))  (NTC on top)
        return R_series_ * (vs / vout - 1.0f);
    }

    [[nodiscard]] float tempC_from_R(float Rntc) const {
        if (!std::isfinite(Rntc) || Rntc <= 0.0f) return NAN;
        const float lnR  = std::log(Rntc);
        const float invT = A_ + B_ * lnR + C_ * lnR * lnR * lnR; // 1/K
        return (1.0f / invT) - 273.15f;
    }

    // Combined: voltage to temperature
    [[nodiscard]] float tempCFromV(float vout, float vs) const {
        float R = resistanceFromV(vout, vs);
        return tempC_from_R(R);
    }

    // Check if temperature is in typical valid range
    [[nodiscard]] bool isValidRange(float tempC, float minC = -40.0f, float maxC = 125.0f) const {
        return std::isfinite(tempC) && tempC >= minC && tempC <= maxC;
    }

private:
    float R_series_;
    float A_, B_, C_;
};

// Legacy alias for compatibility
using Thermistor = ThermistorSH;

} // namespace util
