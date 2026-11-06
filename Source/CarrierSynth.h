#pragma once
#include "Dsp.h"

namespace zonium
{
// Sintetizador interno (portadora) del vocoder: saw + pulse + sub + ruido, detune, ADSR, filtro, mono/poly con glide.
class CarrierSynth
{
public:
    static constexpr int kVoices = 8;
    static constexpr int kStack = 16;

    struct Params
    {
        float saw = 0.8f, pulse = 0.45f, pulseWidth = 0.35f, sub = 0.25f, noise = 0.0f;
        float detuneCents = 10.0f;
        float attackMs = 3.0f, decayMs = 150.0f, sustain = 0.9f, releaseMs = 60.0f;
        float glideMs = 0.0f;
        float cutoffHz = 7000.0f, resonance = 0.15f;
        bool mono = false;
    };

    void prepare (double sr)
    {
        sampleRate = sr;
        allNotesOff();
        lp.reset();
        lastCut = lastRes = -1.0f;
        setParams (params);
    }

    void setParams (const Params& p)
    {
        if (p.mono != params.mono) allNotesOff();
        params = p;
        if (p.cutoffHz != lastCut || p.resonance != lastRes)
        {
            lp.setLowpass (sampleRate, p.cutoffHz, 0.707 + p.resonance * 7.0);
            lastCut = p.cutoffHz; lastRes = p.resonance;
        }
        detuneUp = std::pow (2.0, p.detuneCents / 1200.0);
        detuneDown = 1.0 / detuneUp;
        attInc = 1.0f / (float) std::max (1.0, p.attackMs * 0.001 * sampleRate);
        decDec = (1.0f - p.sustain) / (float) std::max (1.0, p.decayMs * 0.001 * sampleRate);
        relCoef = (float) std::exp (-9.2 / std::max (2.0, p.releaseMs * 0.001 * sampleRate));
        glideCoef = p.glideMs <= 0.0f ? 1.0 : 1.0 - std::exp (-1.0 / (p.glideMs * 0.001 * sampleRate));
    }

    void setPitchBendSemis (float s) noexcept { bendRatio = std::pow (2.0, s / 12.0); }

    void noteOn (int note, float vel)
    {
        if (params.mono) { monoNoteOn (note, vel); return; }

        Voice* v = nullptr;
        for (auto& x : voices) if (x.active && x.note == note) { v = &x; break; }
        if (! v) for (auto& x : voices) if (! x.active) { v = &x; break; }
        if (! v)
        {
            v = &voices[0];
            for (auto& x : voices)
            {
                const bool xr = x.stage == 4, br = v->stage == 4;
                if (xr != br) { if (xr) v = &x; }
                else if (xr ? x.env < v->env : x.age < v->age) v = &x;
            }
        }
        v->active = true; v->note = note; v->vel = vel; v->held = true;
        v->pitch = v->targetPitch = note;
        v->freq = midiToFreq (note);
        v->stage = 1;
        v->age = ++ageCounter;
    }

    void noteOff (int note)
    {
        if (params.mono) { monoNoteOff (note); return; }
        for (auto& x : voices)
            if (x.active && x.held && x.note == note) { x.held = false; x.stage = 4; }
    }

    void allNotesOff()
    {
        for (auto& x : voices) { x = Voice(); }
        stackSize = 0;
    }

    int getActiveVoices() const noexcept
    {
        int n = 0;
        for (auto& x : voices) if (x.active) ++n;
        return n;
    }

    // devuelve la señal portadora (mono) y "gate" (0..1: hay voz sonando)
    float process (float& gate) noexcept
    {
        float sum = 0.0f, g = 0.0f;
        for (auto& v : voices)
        {
            if (! v.active) continue;
            sum += renderVoice (v);
            g = std::max (g, v.env);
        }
        gate = std::min (1.0f, g);
        return lp.process (sum * 0.3f);
    }

private:
    struct Voice
    {
        bool active = false, held = false;
        int note = -1, stage = 0; // 0 idle, 1 attack, 2 decay, 3 sustain, 4 release
        float vel = 1.0f, env = 0.0f;
        double pitch = 69.0, targetPitch = 69.0, freq = 440.0;
        double ph1 = 0.0, ph2 = 0.37, phs = 0.0;
        uint64_t age = 0;
    };

    static float blep (double t, double dt) noexcept
    {
        if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }
    static float sawOsc (double ph, double dt) noexcept { return (float) (2.0 * ph - 1.0) - blep (ph, dt); }
    static float pulseOsc (double ph, double dt, double pw) noexcept
    {
        float v = ph < pw ? 1.0f : -1.0f;
        v += blep (ph, dt);
        double t2 = ph + 1.0 - pw; if (t2 >= 1.0) t2 -= 1.0;
        v -= blep (t2, dt);
        return v;
    }

    float renderVoice (Voice& v) noexcept
    {
        if (v.pitch != v.targetPitch)
        {
            v.pitch += (v.targetPitch - v.pitch) * glideCoef;
            if (std::fabs (v.targetPitch - v.pitch) < 1e-4) v.pitch = v.targetPitch;
            v.freq = midiToFreq (v.pitch);
        }
        const double base = v.freq * bendRatio / sampleRate;
        const double d1 = std::min (0.45, base * detuneUp), d2 = std::min (0.45, base * detuneDown), ds = std::min (0.45, base * 0.5);
        const double pw = params.pulseWidth;

        float s = 0.0f;
        if (params.saw > 0.0f)   s += params.saw * 0.5f * (sawOsc (v.ph1, d1) + sawOsc (v.ph2, d2));
        if (params.pulse > 0.0f) s += params.pulse * 0.35f * (pulseOsc (v.ph1, d1, pw) + pulseOsc (v.ph2, d2, pw));
        if (params.sub > 0.0f)   s += params.sub * 0.5f * pulseOsc (v.phs, ds, 0.5);
        if (params.noise > 0.0f) s += params.noise * 0.5f * noiseGen.next();

        v.ph1 += d1; if (v.ph1 >= 1.0) v.ph1 -= 1.0;
        v.ph2 += d2; if (v.ph2 >= 1.0) v.ph2 -= 1.0;
        v.phs += ds; if (v.phs >= 1.0) v.phs -= 1.0;

        switch (v.stage)
        {
            case 1: v.env += attInc; if (v.env >= 1.0f) { v.env = 1.0f; v.stage = 2; } break;
            case 2: v.env -= decDec; if (v.env <= params.sustain) { v.env = params.sustain; v.stage = 3; } break;
            case 3: v.env = params.sustain; break;
            case 4: v.env *= relCoef; if (v.env < 1e-4f) { v = Voice(); return 0.0f; } break;
            default: break;
        }
        return s * v.env * (0.45f + 0.55f * v.vel);
    }

    bool removeFromStack (int note)
    {
        for (int i = 0; i < stackSize; ++i)
            if (stack[i] == note)
            {
                for (int j = i; j < stackSize - 1; ++j) stack[j] = stack[j + 1];
                --stackSize;
                return true;
            }
        return false;
    }

    void monoNoteOn (int note, float vel)
    {
        removeFromStack (note);
        if (stackSize < kStack) stack[stackSize++] = note;
        Voice& v = voices[0];
        v.note = note; v.vel = vel; v.held = true; v.targetPitch = note;
        if (! v.active)
        {
            v.pitch = note; v.freq = midiToFreq (note); v.env = 0.0f;
            v.active = true; v.stage = 1; v.age = ++ageCounter;
        }
        else if (v.stage == 4) { v.stage = 1; }
        if (params.glideMs <= 0.0f) { v.pitch = note; v.freq = midiToFreq (note); }
    }

    void monoNoteOff (int note)
    {
        if (! removeFromStack (note)) return;
        Voice& v = voices[0];
        if (stackSize > 0)
        {
            v.note = stack[stackSize - 1]; v.targetPitch = v.note;
            if (params.glideMs <= 0.0f) { v.pitch = v.note; v.freq = midiToFreq (v.note); }
        }
        else if (v.active) { v.held = false; v.stage = 4; }
    }

    double sampleRate = 44100.0;
    Params params;
    Voice voices[kVoices];
    int stack[kStack] {};
    int stackSize = 0;
    uint64_t ageCounter = 0;
    double detuneUp = 1.0, detuneDown = 1.0, bendRatio = 1.0, glideCoef = 1.0;
    float attInc = 0.01f, decDec = 0.001f, relCoef = 0.999f, lastCut = -1.0f, lastRes = -1.0f;
    Biquad lp;
    Noise noiseGen;
};
} // namespace zonium
