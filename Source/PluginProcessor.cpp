#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
struct PresetValue { const char* id; float value; };
struct Preset { const char* name; std::vector<PresetValue> values; };

// Todos los presets parten de los valores por defecto y sobrescriben solo lo indicado.
const std::vector<Preset>& presets()
{
    static const std::vector<Preset> p = {
        { "Dabeull Talkbox", {} },
        { "Daft Robot",      { { "bands", 16 }, { "saw", 1.0f }, { "pulse", 0.0f }, { "sub", 0.1f }, { "detune", 4 }, { "vRelease", 20 },
                               { "cutoff", 9000 }, { "drive", 0.45f }, { "chorus", 0.2f }, { "sibilance", 0.7f } } },
        { "Funk Mono Lead",  { { "mono", 1 }, { "glide", 60 }, { "saw", 0.6f }, { "pulse", 0.6f }, { "pw", 0.25f }, { "sub", 0.35f },
                               { "chorus", 0.65f }, { "drive", 0.3f }, { "cutoff", 5000 }, { "res", 0.3f } } },
        { "Dark Voice",      { { "formant", -5 }, { "sub", 0.6f }, { "saw", 0.7f }, { "pulse", 0.2f }, { "cutoff", 3500 },
                               { "chorus", 0.4f }, { "clarity", 0.35f }, { "sibilance", 0.3f } } },
        { "Choir Pad",       { { "cAttack", 80 }, { "cRelease", 450 }, { "cSustain", 1.0f }, { "detune", 20 }, { "chorus", 0.85f },
                               { "bands", 24 }, { "clarity", 0.4f }, { "vRelease", 70 }, { "drive", 0.1f } } },
        { "Bright Vocoder",  { { "bands", 28 }, { "formant", 3 }, { "clarity", 0.85f }, { "sibilance", 0.85f }, { "cutoff", 12000 },
                               { "saw", 1.0f }, { "pulse", 0.3f }, { "chorus", 0.3f }, { "res", 0.1f } } },
    };
    return p;
}
} // namespace

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout VoxMachineProcessor::createLayout()
{
    APVTS::ParameterLayout layout;
    using F = juce::AudioParameterFloat;
    using Attr = juce::AudioParameterFloatAttributes;

    auto fl = [&] (const char* id, const char* name, float lo, float hi, float def, float skew, const char* unit, int dec)
    {
        const juce::String u (unit);
        layout.add (std::make_unique<F> (juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (lo, hi, 0.0f, skew), def,
                                         Attr().withStringFromValueFunction ([u, dec] (float v, int) { return juce::String (v, dec) + u; })));
    };
    auto pct = [&] (const char* id, const char* name, float def, float lo = 0.0f, float hi = 1.0f)
    {
        layout.add (std::make_unique<F> (juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (lo, hi), def,
                                         Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })));
    };

    fl ("inGain", "Voice Level", -24.0f, 24.0f, 0.0f, 1.0f, " dB", 1);
    fl ("outGain", "Output", -24.0f, 12.0f, 0.0f, 1.0f, " dB", 1);
    pct ("mix", "Dry/Wet", 1.0f);

    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "bands", 1 }, "Bands", 8, 32, 20));
    fl ("formant", "Formant", -12.0f, 12.0f, 0.0f, 1.0f, " st", 1);
    fl ("vAttack", "Env Attack", 0.5f, 50.0f, 2.0f, 0.5f, " ms", 1);
    fl ("vRelease", "Env Release", 5.0f, 400.0f, 35.0f, 0.5f, " ms", 0);
    pct ("clarity", "Clarity", 0.55f);
    pct ("sibilance", "Sibilance", 0.5f);

    pct ("saw", "Saw", 0.8f);
    pct ("pulse", "Pulse", 0.45f);
    pct ("pw", "Pulse Width", 0.35f, 0.1f, 0.5f);
    pct ("sub", "Sub", 0.25f);
    pct ("noise", "Noise", 0.0f);
    fl ("detune", "Detune", 0.0f, 50.0f, 10.0f, 1.0f, " ct", 0);
    fl ("cutoff", "Cutoff", 200.0f, 16000.0f, 7000.0f, 0.3f, " Hz", 0);
    pct ("res", "Resonance", 0.15f);

    fl ("cAttack", "Attack", 0.5f, 2000.0f, 3.0f, 0.3f, " ms", 0);
    fl ("cDecay", "Decay", 5.0f, 3000.0f, 150.0f, 0.3f, " ms", 0);
    pct ("cSustain", "Sustain", 0.9f);
    fl ("cRelease", "Release", 5.0f, 4000.0f, 60.0f, 0.3f, " ms", 0);
    fl ("glide", "Glide", 0.0f, 1000.0f, 0.0f, 0.4f, " ms", 0);
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "mono", 1 }, "Mono", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "loop", 1 }, "Loop Vocal", true));

    pct ("drive", "Drive", 0.25f);
    pct ("chorus", "Chorus", 0.5f);
    return layout;
}

VoxMachineProcessor::VoxMachineProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VoxMachine", createLayout())
{
    auto g = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pIn = g ("inGain"); pOut = g ("outGain"); pMix = g ("mix"); pBands = g ("bands"); pFormant = g ("formant");
    pVAtk = g ("vAttack"); pVRel = g ("vRelease"); pClarity = g ("clarity"); pSib = g ("sibilance");
    pSaw = g ("saw"); pPulse = g ("pulse"); pPw = g ("pw"); pSub = g ("sub"); pNoise = g ("noise");
    pDetune = g ("detune"); pCutoff = g ("cutoff"); pRes = g ("res");
    pCAtk = g ("cAttack"); pCDec = g ("cDecay"); pCSus = g ("cSustain"); pCRel = g ("cRelease");
    pLoop = g ("loop"); pGlide = g ("glide"); pMono = g ("mono"); pDrive = g ("drive"); pChorus = g ("chorus");
}

bool VoxMachineProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void VoxMachineProcessor::prepareToPlay (double sr, int)
{
    hostSr = sr;
    vocoder.prepare (sr);
    synth.prepare (sr);
    chorus.prepare (sr);
    mixSm.reset (sr, 0.02); outSm.reset (sr, 0.02);
    mixSm.setCurrentAndTargetValue (*pMix);
    outSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOut->load()));
    updateParams();
}

void VoxMachineProcessor::updateParams()
{
    zonium::Vocoder::Params vp;
    vp.bands = (int) pBands->load();
    vp.formantSemis = pFormant->load();
    vp.attackMs = pVAtk->load();
    vp.releaseMs = pVRel->load();
    vp.clarity = pClarity->load();
    vp.sibilance = pSib->load();
    vocoder.setParams (vp);

    zonium::CarrierSynth::Params cp;
    cp.saw = pSaw->load(); cp.pulse = pPulse->load(); cp.pulseWidth = pPw->load(); cp.sub = pSub->load(); cp.noise = pNoise->load();
    cp.detuneCents = pDetune->load();
    cp.attackMs = pCAtk->load(); cp.decayMs = pCDec->load(); cp.sustain = pCSus->load(); cp.releaseMs = pCRel->load();
    cp.glideMs = pGlide->load();
    cp.cutoffHz = pCutoff->load(); cp.resonance = pRes->load();
    cp.mono = pMono->load() > 0.5f;
    synth.setParams (cp);

    inGainLin = juce::Decibels::decibelsToGain (pIn->load());
    driveAmt = pDrive->load();
    chorusAmt = pChorus->load();
    mixSm.setTargetValue (pMix->load());
    outSm.setTargetValue (juce::Decibels::decibelsToGain (pOut->load()));
}

void VoxMachineProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
    {
        const int note = m.getNoteNumber() & 127;
        if (heldCount == 0) { pos = 0.0; playing = lastSample != nullptr; } // primera tecla: el vocal arranca
        if (! held[(size_t) note]) { held[(size_t) note] = true; ++heldCount; }
        synth.noteOn (note, m.getFloatVelocity());
    }
    else if (m.isNoteOff())
    {
        const int note = m.getNoteNumber() & 127;
        if (held[(size_t) note]) { held[(size_t) note] = false; --heldCount; }
        synth.noteOff (note);
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff()) { synth.allNotesOff(); held.fill (false); heldCount = 0; }
    else if (m.isPitchWheel()) synth.setPitchBendSemis ((float) (m.getPitchWheelValue() - 8192) / 8192.0f * 2.0f);
    else return;
    midiFlash.store (1);
}

void VoxMachineProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int ns = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2) return;

    updateParams();

    const Sample* smp = current.load (std::memory_order_acquire);
    if (smp != lastSample) { lastSample = smp; playing = false; pos = 0.0; }
    const bool loop = pLoop->load() > 0.5f;
    const int len = smp ? (int) smp->data.size() : 0;
    const double step = smp ? smp->sampleRate / hostSr : 1.0;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    auto it = midi.begin();
    float inPk = 0.0f, outPk = 0.0f;

    for (int n = 0; n < ns; ++n)
    {
        while (it != midi.end() && (*it).samplePosition <= n) { handleMidi ((*it).getMessage()); ++it; }

        // modulador = el vocal cargado (mientras hay teclas pulsadas)
        float mod = 0.0f;
        if (playing && smp != nullptr && len > 1)
        {
            const int i0 = juce::jlimit (0, len - 1, (int) pos);
            const int i1 = i0 + 1 < len ? i0 + 1 : (loop ? 0 : i0);
            const float fr = (float) (pos - (double) i0);
            mod = (smp->data[(size_t) i0] + (smp->data[(size_t) i1] - smp->data[(size_t) i0]) * fr) * inGainLin;
            pos += step;
            if (pos >= (double) len)
            {
                if (loop) pos -= (double) len; else playing = false;
            }
        }
        inPk = juce::jmax (inPk, std::abs (mod));

        float gate = 0.0f;
        const float car = synth.process (gate);
        float v = vocoder.process (mod, car, gate);
        v = zonium::softDrive (v, driveAmt);

        float wl, wr;
        chorus.process (v, chorusAmt, wl, wr);

        const float mix = mixSm.getNextValue();
        const float og = outSm.getNextValue();
        L[n] = mod * (1.0f - mix) + wl * mix * og;
        R[n] = mod * (1.0f - mix) + wr * mix * og;
        outPk = juce::jmax (outPk, std::abs (L[n]), std::abs (R[n]));
    }

    if (heldCount == 0 && synth.getActiveVoices() == 0) playing = false;
    playPos.store (playing && len > 0 ? (float) (pos / (double) len) : 0.0f);

    inPeak.store (inPk);
    outPeak.store (outPk);
    activeVoices.store (synth.getActiveVoices());
    const int nb = vocoder.getNumBands();
    bandsShown.store (nb);
    const float* lv = vocoder.getLevels();
    for (int i = 0; i < nb; ++i) bandLevels[(size_t) i].store (lv[i]);
}

bool VoxMachineProcessor::loadSample (const juce::File& file)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->numChannels == 0) return false;
    if ((double) reader->lengthInSamples > reader->sampleRate * 600.0) return false; // máx. 10 min

    const int len = (int) reader->lengthInSamples;
    const int nch = (int) reader->numChannels;
    juce::AudioBuffer<float> tmp (nch, len);
    if (! reader->read (&tmp, 0, len, 0, true, true)) return false;

    auto s = std::make_unique<Sample>();
    s->data.assign ((size_t) len, 0.0f);
    float peak = 0.0f;
    for (int i = 0; i < len; ++i)
    {
        float sum = 0.0f;
        for (int c = 0; c < nch; ++c) sum += tmp.getSample (c, i);
        sum /= (float) nch;
        s->data[(size_t) i] = sum;
        peak = juce::jmax (peak, std::abs (sum));
    }
    const float g = peak > 1.0e-6f ? 0.9f / peak : 1.0f; // normaliza para un nivel consistente
    for (auto& x : s->data) x *= g;

    const int bins = 600;
    s->peaks.assign ((size_t) bins, 0.0f);
    for (int i = 0; i < len; ++i)
    {
        const size_t b = (size_t) ((int64_t) i * bins / len);
        s->peaks[b] = juce::jmax (s->peaks[b], std::abs (s->data[(size_t) i]));
    }
    s->sampleRate = reader->sampleRate;
    s->name = file.getFileName();
    s->path = file.getFullPathName();

    const Sample* raw = s.get();
    retained.push_back (std::move (s));
    if (retained.size() > 8) retained.erase (retained.begin()); // el hilo de audio ya no usa los antiguos
    current.store (raw, std::memory_order_release);
    return true;
}

int VoxMachineProcessor::getNumPrograms() { return (int) presets().size(); }
const juce::String VoxMachineProcessor::getProgramName (int i) { return presets()[(size_t) juce::jlimit (0, getNumPrograms() - 1, i)].name; }

void VoxMachineProcessor::setCurrentProgram (int index)
{
    index = juce::jlimit (0, getNumPrograms() - 1, index);
    currentProgram = index;
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());
    for (auto& v : presets()[(size_t) index].values)
        if (auto* rp = apvts.getParameter (v.id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (v.value));
}

void VoxMachineProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto* sm = getSample()) state.setProperty ("samplePath", sm->path, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void VoxMachineProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) tree.getProperty ("program", 0);
            apvts.replaceState (tree);
            const juce::String path = tree.getProperty ("samplePath", "").toString();
            if (path.isNotEmpty() && juce::File (path).existsAsFile()
                && (getSample() == nullptr || getSample()->path != path))
                loadSample (juce::File (path));
        }
}

juce::AudioProcessorEditor* VoxMachineProcessor::createEditor() { return new VoxMachineEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VoxMachineProcessor(); }
