#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace zonium
{
constexpr double kPi = 3.14159265358979323846;

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void reset() noexcept { z1 = z2 = 0; }

    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setCoeffs (double a0, double nb0, double nb1, double nb2, double na1, double na2) noexcept
    {
        b0 = (float) (nb0 / a0);
        b1 = (float) (nb1 / a0);
        b2 = (float) (nb2 / a0);
        a1 = (float) (na1 / a0);
        a2 = (float) (na2 / a0);
    }

    void setBandpass (double sr, double f, double q) noexcept
    {
        f = std::clamp (f, 20.0, sr * 0.45);
        const double w = 2.0 * kPi * f / sr, alpha = std::sin (w) / (2.0 * q), c = std::cos (w);
        setCoeffs (1.0 + alpha, alpha, 0.0, -alpha, -2.0 * c, 1.0 - alpha);
    }

    void setLowpass (double sr, double f, double q) noexcept
    {
        f = std::clamp (f, 20.0, sr * 0.45);
        const double w = 2.0 * kPi * f / sr, alpha = std::sin (w) / (2.0 * q), c = std::cos (w);
        setCoeffs (1.0 + alpha, (1.0 - c) * 0.5, 1.0 - c, (1.0 - c) * 0.5, -2.0 * c, 1.0 - alpha);
    }

    void setHighpass (double sr, double f, double q) noexcept
    {
        f = std::clamp (f, 20.0, sr * 0.45);
        const double w = 2.0 * kPi * f / sr, alpha = std::sin (w) / (2.0 * q), c = std::cos (w);
        setCoeffs (1.0 + alpha, (1.0 + c) * 0.5, -(1.0 + c), (1.0 + c) * 0.5, -2.0 * c, 1.0 - alpha);
    }
};

struct Noise
{
    uint32_t s = 22222u;
    float next() noexcept
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) (int32_t) s * (1.0f / 2147483648.0f);
    }
};

inline double midiToFreq (double note) noexcept { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }
} // namespace zonium
