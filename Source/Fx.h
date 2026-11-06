#pragma once
#include "Dsp.h"
#include <vector>

namespace zonium
{
// Chorus estéreo (mono -> estéreo) para ese carácter "ancho" de talkbox funk.
class Chorus
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        size = (int) (sr * 0.06) + 8;
        buf.assign ((size_t) size, 0.0f);
        w = 0; phase = 0.0;
    }

    void process (float in, float amount, float& outL, float& outR) noexcept
    {
        buf[(size_t) w] = in;
        phase += 0.35 / sampleRate; if (phase >= 1.0) phase -= 1.0;
        const float lfoL = (float) std::sin (2.0 * kPi * phase);
        const float lfoR = (float) std::sin (2.0 * kPi * (phase + 0.5));
        const float dL = (float) ((0.012 + 0.0035 * lfoL) * sampleRate);
        const float dR = (float) ((0.012 + 0.0035 * lfoR) * sampleRate);
        const float wl = read (dL), wr = read (dR);
        const float dry = 1.0f - 0.4f * amount;
        outL = in * dry + wl * 0.8f * amount;
        outR = in * dry + wr * 0.8f * amount;
        if (++w >= size) w = 0;
    }

private:
    float read (float d) const noexcept
    {
        float rp = (float) w - d;
        while (rp < 0.0f) rp += (float) size;
        int i0 = (int) rp; if (i0 >= size) i0 = 0;
        int i1 = i0 + 1; if (i1 >= size) i1 = 0;
        const float fr = rp - (float) i0;
        return buf[(size_t) i0] * (1.0f - fr) + buf[(size_t) i1] * fr;
    }

    std::vector<float> buf;
    double sampleRate = 44100.0, phase = 0.0;
    int size = 1, w = 0;
};

inline float softDrive (float x, float amount) noexcept
{
    const float pre = 1.0f + amount * 6.0f;
    return std::tanh (x * pre) / std::tanh (pre);
}
} // namespace zonium
