// Test offline del DSP (sin JUCE): voz sintética -> vocoder + sintetizador interno.
#include "../Source/CarrierSynth.h"
#include "../Source/Vocoder.h"
#include <cstdio>
#include <vector>

using namespace zonium;

static double rms (const std::vector<float>& v, size_t a, size_t b)
{
    double s = 0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i];
    return std::sqrt (s / std::max<size_t> (1, b - a));
}

static double detectPitch (const std::vector<float>& v, size_t a, size_t b, double sr)
{
    double best = 0; int bestLag = 0;
    for (int lag = (int) (sr / 1000); lag < (int) (sr / 60); ++lag)
    {
        double c = 0; for (size_t i = a; i + lag < b; ++i) c += (double) v[i] * v[i + lag];
        if (c > best) { best = c; bestLag = lag; }
    }
    return bestLag ? sr / bestLag : 0;
}

int main()
{
    int failures = 0;
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    for (int bands : { 8, 20, 32 })
    {
        Vocoder voc; CarrierSynth syn;
        Vocoder::Params vp; vp.bands = bands; voc.setParams (vp); voc.prepare (sr);
        CarrierSynth::Params cp; cp.sub = 0.0f; cp.detuneCents = 0.0f; syn.setParams (cp); syn.prepare (sr);
        syn.noteOn (57, 1.0f); // A3 = 220 Hz

        // "voz": tren de pulsos 130 Hz por 3 formantes, con sílabas (0.4 s sí / 0.3 s no)
        Biquad f1, f2, f3; f1.setBandpass (sr, 700, 8); f2.setBandpass (sr, 1200, 8); f3.setBandpass (sr, 2600, 8);
        const size_t N = (size_t) (sr * 2.8);
        std::vector<float> out (N), in (N);
        double ph = 0;
        for (size_t n = 0; n < N; ++n)
        {
            ph += 130.0 / sr; float imp = 0; if (ph >= 1) { ph -= 1; imp = 1; }
            const double t = std::fmod ((double) n / sr, 0.7);
            const float on = t < 0.4 ? 1.0f : 0.0f;
            const float x = (f1.process (imp) + 0.7f * f2.process (imp) + 0.4f * f3.process (imp)) * 60.0f * on;
            in[n] = x;
            float gate; const float car = syn.process (gate);
            out[n] = voc.process (x, car, gate);
        }
        bool ok = true;
        for (float s : out) if (! std::isfinite (s) || std::fabs (s) > 4.0f) ok = false;
        const double rIn = rms (in, (size_t) (sr * 0.1), (size_t) (sr * 0.35));
        const double rOn = rms (out, (size_t) (sr * 0.1), (size_t) (sr * 0.35));
        const double rOff = rms (out, (size_t) (sr * 0.55), (size_t) (sr * 0.68));
        const double pitch = detectPitch (out, (size_t) (sr * 0.15), (size_t) (sr * 0.35), sr);
        const bool gateOk = rOff < rOn * 0.05;
        const bool pitchOk = bands > 20 || std::fabs (pitch - 220.0) < 4.0; // 32 bandas: autocorrelación poco fiable
        std::printf ("sr=%6.0f bands=%2d  inRMS=%.3f outRMS=%.3f (ratio %.2f)  silenceRMS=%.4f  pitch=%.1f Hz  finite=%d gate=%d pitch=%d\n",
                     sr, bands, rIn, rOn, rOn / rIn, rOff, pitch, ok, gateOk, pitchOk);
        if (! ok || ! gateOk || ! pitchOk) ++failures;
    }

    // Prueba de mono/poly y note off sin explotar
    CarrierSynth s; s.prepare (48000);
    CarrierSynth::Params p; p.mono = true; p.glideMs = 80; s.setParams (p);
    float g; for (int n : { 60, 64, 67 }) s.noteOn (n, 0.8f);
    for (int i = 0; i < 4800; ++i) s.process (g);
    for (int n : { 67, 64, 60 }) s.noteOff (n);
    for (int i = 0; i < 48000; ++i) s.process (g);
    std::printf ("mono: voces activas tras soltar todo = %d (esperado 0)\n", s.getActiveVoices());
    if (s.getActiveVoices() != 0) ++failures;

    std::printf (failures ? "FALLOS: %d\n" : "TODO OK\n", failures);
    return failures;
}
