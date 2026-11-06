#pragma once
#include "Dsp.h"

namespace zonium
{
// Vocoder de bandas: la voz (modulador) controla la amplitud de cada banda de la portadora (sintetizador).
class Vocoder
{
public:
    static constexpr int kMaxBands = 32;

    struct Params
    {
        int bands = 20;
        float formantSemis = 0.0f, attackMs = 2.0f, releaseMs = 35.0f, clarity = 0.55f, sibilance = 0.5f;
    };

    void prepare (double sr)
    {
        sampleRate = sr;
        hpMod.setHighpass (sr, 5500.0, 0.707);
        hpNoise.setHighpass (sr, 5500.0, 0.707);
        reset();
        apply();
    }

    void reset()
    {
        for (int i = 0; i < kMaxBands; ++i)
        {
            anA[i].reset(); anB[i].reset(); syA[i].reset(); syB[i].reset(); env[i] = 0.0f;
        }
        hpMod.reset(); hpNoise.reset(); sibEnv = 0.0f;
    }

    void setParams (const Params& p)
    {
        if (p.bands == params.bands && p.formantSemis == params.formantSemis && p.attackMs == params.attackMs
            && p.releaseMs == params.releaseMs && p.clarity == params.clarity && p.sibilance == params.sibilance)
            return;
        params = p;
        apply();
    }

    int getNumBands() const noexcept { return nBands; }
    const float* getLevels() const noexcept { return env; }

    float process (float mod, float car, float gate) noexcept
    {
        float out = 0.0f;
        for (int i = 0; i < nBands; ++i)
        {
            const float m = anB[i].process (anA[i].process (mod));
            const float r = std::fabs (m);
            env[i] += (r > env[i] ? atkC : relC) * (r - env[i]);
            const float c = syB[i].process (syA[i].process (car));
            out += c * env[i] * gain[i];
        }

        const float h = std::fabs (hpMod.process (mod));
        sibEnv += (h > sibEnv ? atkC : relC) * (h - sibEnv);
        out += hpNoise.process (noise.next()) * sibEnv * params.sibilance * 3.0f * gate;

        if (! std::isfinite (out)) { reset(); return 0.0f; }
        return out;
    }

private:
    void apply()
    {
        const double lo = 90.0, hi = std::min (9000.0, sampleRate * 0.42);
        nBands = std::clamp (params.bands, 4, kMaxBands);
        const double ratio = std::pow (hi / lo, 1.0 / (nBands - 1));
        const double q = 0.8 / (2.0 * std::sinh (std::log (ratio) * 0.5));
        const double shift = std::pow (2.0, params.formantSemis / 12.0);
        const float tilt = 0.6f * params.clarity;
        const float makeup = makeupBase;

        for (int i = 0; i < nBands; ++i)
        {
            const double f = lo * std::pow (ratio, (double) i);
            anA[i].setBandpass (sampleRate, f, q); anB[i].setBandpass (sampleRate, f, q);
            syA[i].setBandpass (sampleRate, f * shift, q); syB[i].setBandpass (sampleRate, f * shift, q);
            gain[i] = makeup * (float) std::pow (f / 1000.0, (double) tilt);
        }
        atkC = 1.0f - (float) std::exp (-1.0 / (std::max (0.1f, params.attackMs) * 0.001 * sampleRate));
        relC = 1.0f - (float) std::exp (-1.0 / (std::max (1.0f, params.releaseMs) * 0.001 * sampleRate));
    }

    static constexpr float makeupBase = 70.0f;

    double sampleRate = 44100.0;
    Params params;
    int nBands = 20;
    Biquad anA[kMaxBands], anB[kMaxBands], syA[kMaxBands], syB[kMaxBands], hpMod, hpNoise;
    float env[kMaxBands] {}, gain[kMaxBands] {};
    float atkC = 0.1f, relC = 0.01f, sibEnv = 0.0f;
    Noise noise;
};
} // namespace zonium
