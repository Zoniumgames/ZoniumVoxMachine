#include "PluginEditor.h"

namespace
{
juce::Font font (float h, int style = juce::Font::plain) { return juce::Font (juce::FontOptions (h, style)); }
float toMeter (float lin) { return juce::jlimit (0.0f, 1.0f, juce::jmap (juce::Decibels::gainToDecibels (lin, -60.0f), -60.0f, 0.0f, 0.0f, 1.0f)); }
}

VoxMachineEditor::VoxMachineEditor (VoxMachineProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    auto& s = p.apvts;

    auto add = [&] (std::vector<std::unique_ptr<Knob>>& grp, const char* id, const char* name)
    {
        grp.push_back (std::make_unique<Knob> (s, id, name));
        addAndMakeVisible (*grp.back());
    };

    add (vocK, "bands", "BANDS");      add (vocK, "formant", "FORMANT");  add (vocK, "vAttack", "ENV ATTACK");
    add (vocK, "vRelease", "ENV RELEASE"); add (vocK, "clarity", "CLARITY"); add (vocK, "sibilance", "SIBILANCE");

    add (outK, "inGain", "VOICE LVL");  add (outK, "drive", "DRIVE");  add (outK, "chorus", "CHORUS");
    add (outK, "outGain", "OUTPUT"); add (outK, "mix", "DRY / WET");

    add (carK, "saw", "SAW");   add (carK, "pulse", "PULSE"); add (carK, "pw", "PULSE W.");  add (carK, "sub", "SUB");
    add (carK, "noise", "NOISE"); add (carK, "detune", "DETUNE"); add (carK, "cutoff", "CUTOFF"); add (carK, "res", "RESONANCE");

    add (envK, "cAttack", "ATTACK");  add (envK, "cDecay", "DECAY");  add (envK, "cSustain", "SUSTAIN");
    add (envK, "cRelease", "RELEASE"); add (envK, "glide", "GLIDE");

    monoBtn.setButtonText ("MONO");
    addAndMakeVisible (monoBtn);
    monoAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, "mono", monoBtn);

    for (int i = 0; i < p.getNumPrograms(); ++i) presetBox.addItem (p.getProgramName (i), i + 1);
    presetBox.setSelectedItemIndex (juce::jlimit (0, p.getNumPrograms() - 1, p.getCurrentProgram()), juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedItemIndex()); };
    addAndMakeVisible (presetBox);

    loadBtn.setButtonText ("LOAD VOCAL");
    loadBtn.setColour (juce::TextButton::buttonColourId, ui::gold);
    loadBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff1a1305));
    loadBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a vocal", juce::File(), "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) loadFile (fc.getResult()); });
    };
    addAndMakeVisible (loadBtn);
    loopBtn.setButtonText ("LOOP");
    addAndMakeVisible (loopBtn);
    loopAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, "loop", loopBtn);

    setSize (1040, 560);
    startTimerHz (30);
}

VoxMachineEditor::~VoxMachineEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void VoxMachineEditor::layoutRow (juce::Rectangle<int> r, std::vector<juce::Component*> items)
{
    auto inner = r.reduced (10, 8);
    inner.removeFromTop (22);
    const int w = inner.getWidth() / (int) items.size();
    for (auto* c : items) c->setBounds (inner.removeFromLeft (w));
}

void VoxMachineEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    auto header = area.removeFromTop (60);
    presetBox.setBounds (header.removeFromRight (230).withSizeKeepingCentre (230, 30));
    header.removeFromRight (10);
    loopBtn.setBounds (header.removeFromRight (70).withSizeKeepingCentre (70, 30));
    header.removeFromRight (8);
    loadBtn.setBounds (header.removeFromRight (130).withSizeKeepingCentre (130, 30));
    area.removeFromTop (6);
    displayRect = area.removeFromTop (92);
    area.removeFromTop (10);
    bottomRect = area.removeFromBottom (34);
    area.removeFromBottom (8);

    const int rowH = (area.getHeight() - 10) / 2;
    auto rowA = area.removeFromTop (rowH);
    area.removeFromTop (10);
    auto rowB = area;

    vocR = rowA.removeFromLeft ((rowA.getWidth() - 12) * 6 / 11);
    rowA.removeFromLeft (12);
    outR = rowA;
    carR = rowB.removeFromLeft ((rowB.getWidth() - 12) * 8 / 14);
    rowB.removeFromLeft (12);
    envR = rowB;

    auto toComps = [] (std::vector<std::unique_ptr<Knob>>& v) { std::vector<juce::Component*> c; for (auto& k : v) c.push_back (k.get()); return c; };
    layoutRow (vocR, toComps (vocK));
    layoutRow (outR, toComps (outK));
    layoutRow (carR, toComps (carK));
    auto envComps = toComps (envK);
    envComps.push_back (&monoBtn);
    layoutRow (envR, envComps);
}

void VoxMachineEditor::drawPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title)
{
    g.setColour (ui::panel);
    g.fillRoundedRectangle (r.toFloat(), 10.0f);
    g.setColour (ui::panelEdge);
    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 10.0f, 1.0f);
    g.setColour (ui::gold);
    g.setFont (font (12.0f, juce::Font::bold));
    g.drawText (title, r.getX() + 14, r.getY() + 7, r.getWidth() - 28, 16, juce::Justification::left);
}

void VoxMachineEditor::drawMeter (juce::Graphics& g, juce::Rectangle<float> r, float level)
{
    g.setColour (juce::Colour (0xff0d0d14));
    g.fillRoundedRectangle (r, 3.0f);
    auto fill = r.reduced (1.0f);
    fill.setWidth (fill.getWidth() * level);
    g.setGradientFill (juce::ColourGradient (ui::cyan, r.getX(), 0.0f, ui::gold, r.getRight(), 0.0f, false));
    g.fillRoundedRectangle (fill, 2.0f);
}

void VoxMachineEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (ui::bgTop, 0.0f, 0.0f, ui::bgBottom, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // cabecera
    g.setColour (ui::gold);
    g.setFont (font (11.5f, juce::Font::bold));
    g.drawText ("Z O N I U M G A M E S", 16, 14, 300, 14, juce::Justification::left);
    g.setColour (juce::Colours::white);
    g.setFont (font (30.0f, juce::Font::bold));
    g.drawText ("VOXMACHINE", 16, 28, 260, 36, juce::Justification::left);
    g.setColour (ui::cyan);
    g.setFont (font (12.0f, juce::Font::bold));
    g.drawText ("TALKBOX  VOCODER", 214, 42, 200, 16, juce::Justification::left);

    // vocal (forma de onda) + bandas del vocoder
    const auto* smp = proc.getSample();
    drawPanel (g, displayRect, smp ? "VOCAL:  " + smp->name + "      |      VOCODER BANDS" : "VOCAL      |      VOCODER BANDS");
    auto inner = displayRect.reduced (14, 8);
    inner.removeFromTop (18);
    auto wave = inner.removeFromLeft (inner.getWidth() * 55 / 100);
    inner.removeFromLeft (14);
    auto bar = inner;

    g.setColour (juce::Colour (0xff0d0d14));
    g.fillRoundedRectangle (wave.toFloat(), 6.0f);
    if (smp != nullptr && ! smp->peaks.empty())
    {
        g.setGradientFill (juce::ColourGradient (ui::gold, 0.0f, (float) wave.getY(), ui::cyan, 0.0f, (float) wave.getBottom(), false));
        const float cy = (float) wave.getCentreY();
        for (int x = 0; x < wave.getWidth(); ++x)
        {
            const float pk = smp->peaks[(size_t) ((int64_t) x * (int64_t) smp->peaks.size() / wave.getWidth())];
            const float h = juce::jmax (1.0f, pk * (float) wave.getHeight() * 0.5f);
            g.fillRect ((float) (wave.getX() + x), cy - h, 1.0f, h * 2.0f);
        }
        if (playPos > 0.0f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.fillRect ((float) wave.getX() + playPos * (float) wave.getWidth(), (float) wave.getY(), 2.0f, (float) wave.getHeight());
        }
    }
    else
    {
        g.setColour (ui::dim);
        g.setFont (font (13.0f, juce::Font::bold));
        g.drawText ("DRAG A VOCAL HERE  (WAV / AIFF / MP3)", wave, juce::Justification::centred);
    }
    if (dragOver)
    {
        g.setColour (ui::gold);
        g.drawRoundedRectangle (wave.toFloat(), 6.0f, 2.0f);
    }

    const float bw = (float) bar.getWidth() / (float) nBands;
    g.setGradientFill (juce::ColourGradient (ui::gold, 0.0f, (float) bar.getY(), ui::cyan, 0.0f, (float) bar.getBottom(), false));
    for (int i = 0; i < nBands; ++i)
    {
        const float h = toMeter (bands[i] * 2.0f) * (float) bar.getHeight();
        g.fillRoundedRectangle ((float) bar.getX() + bw * (float) i + 1.5f, (float) bar.getBottom() - h, bw - 3.0f, juce::jmax (2.0f, h), 2.0f);
    }

    drawPanel (g, vocR, "VOCODER");
    drawPanel (g, outR, "OUTPUT");
    drawPanel (g, carR, "CARRIER SYNTH");
    drawPanel (g, envR, "CARRIER ENVELOPE");

    // barra inferior
    auto b = bottomRect;
    g.setColour (ui::dim);
    g.setFont (font (11.0f, juce::Font::bold));
    g.drawText ("VOX", b.getX(), b.getY(), 30, b.getHeight(), juce::Justification::centredLeft);
    drawMeter (g, { (float) b.getX() + 34.0f, (float) b.getCentreY() - 4.0f, 170.0f, 8.0f }, toMeter (inLevel));
    g.drawText ("OUT", b.getX() + 210, b.getY(), 30, b.getHeight(), juce::Justification::centredLeft);
    drawMeter (g, { (float) b.getX() + 244.0f, (float) b.getCentreY() - 4.0f, 170.0f, 8.0f }, toMeter (outLevel));

    const float cx = (float) b.getX() + 450.0f, cy = (float) b.getCentreY();
    g.setColour (ui::gold.withAlpha (0.15f + 0.85f * led));
    g.fillEllipse (cx, cy - 5.0f, 10.0f, 10.0f);
    g.setColour (ui::dim);
    g.drawText ("MIDI", (int) cx + 16, b.getY(), 40, b.getHeight(), juce::Justification::centredLeft);
    g.drawText ("VOICES " + juce::String (voices), (int) cx + 70, b.getY(), 90, b.getHeight(), juce::Justification::centredLeft);
    g.drawText ("Zonium VoxMachine v" + juce::String (VoxMachineProcessor::kVersion) + "  -  Zoniumgames", b.withTrimmedLeft (b.getWidth() - 320),
                juce::Justification::centredRight);
}

void VoxMachineEditor::timerCallback()
{
    nBands = juce::jlimit (4, 32, proc.bandsShown.load());
    for (int i = 0; i < nBands; ++i)
    {
        const float v = proc.bandLevels[(size_t) i].load();
        bands[i] = juce::jmax (v, bands[i] * 0.85f);
    }
    inLevel = juce::jmax (proc.inPeak.load(), inLevel * 0.88f);
    outLevel = juce::jmax (proc.outPeak.load(), outLevel * 0.88f);
    if (proc.midiFlash.exchange (0) != 0) led = 1.0f; else led *= 0.85f;
    voices = proc.activeVoices.load();
    playPos = proc.playPos.load();
    presetBox.setSelectedItemIndex (juce::jlimit (0, proc.getNumPrograms() - 1, proc.getCurrentProgram()), juce::dontSendNotification);
    repaint();
}

bool VoxMachineEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;mp3;flac;ogg")) return true;
    return false;
}

void VoxMachineEditor::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;mp3;flac;ogg")) { loadFile (juce::File (f)); break; }
    repaint();
}

void VoxMachineEditor::loadFile (const juce::File& f)
{
    if (! proc.loadSample (f))
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Zonium VoxMachine",
                                                "Could not load that audio file.");
    repaint();
}
