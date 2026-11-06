#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <atomic>
#include "CarrierSynth.h"
#include "Vocoder.h"
#include "Fx.h"

class VoxMachineProcessor : public juce::AudioProcessor
{
public:
    VoxMachineProcessor();
    ~VoxMachineProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Zonium VoxMachine"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.3; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // Vocal cargado (se lee en el hilo de interfaz; el hilo de audio solo usa el puntero publicado)
    struct Sample
    {
        std::vector<float> data, peaks;
        double sampleRate = 44100.0;
        juce::String name, path;
    };
    bool loadSample (const juce::File& file);
    const Sample* getSample() const noexcept { return current.load (std::memory_order_acquire); }
    std::atomic<float> playPos { 0.0f };

    // datos para la interfaz (escritos desde el hilo de audio, leídos por el editor)
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };
    std::atomic<int> midiFlash { 0 }, activeVoices { 0 }, bandsShown { 20 };
    std::array<std::atomic<float>, 32> bandLevels {};

    static constexpr const char* kVersion = "1.0.0";

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleMidi (const juce::MidiMessage&);
    void updateParams();

    zonium::Vocoder vocoder;
    zonium::CarrierSynth synth;
    zonium::Chorus chorus;
    juce::SmoothedValue<float> mixSm, outSm;

    std::atomic<float> *pIn, *pOut, *pMix, *pBands, *pFormant, *pVAtk, *pVRel, *pClarity, *pSib, *pSaw, *pPulse, *pPw,
        *pSub, *pNoise, *pDetune, *pCutoff, *pRes, *pCAtk, *pCDec, *pCSus, *pCRel, *pGlide, *pMono, *pDrive, *pChorus;

    std::atomic<const Sample*> current { nullptr };
    std::vector<std::unique_ptr<Sample>> retained;
    const Sample* lastSample = nullptr;
    std::array<bool, 128> held {};
    int heldCount = 0;
    double pos = 0.0, hostSr = 44100.0;
    bool playing = false;
    std::atomic<float>* pLoop = nullptr;

    float inGainLin = 1.0f, driveAmt = 0.0f, chorusAmt = 0.0f;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxMachineProcessor)
};
