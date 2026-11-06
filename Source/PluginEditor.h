#pragma once
#include "PluginProcessor.h"
#include <memory>
#include <vector>

namespace ui
{
const juce::Colour bgTop    { 0xff15121f };
const juce::Colour bgBottom { 0xff08080d };
const juce::Colour panel    { 0xff1b1a29 };
const juce::Colour panelEdge{ 0xff2e2c44 };
const juce::Colour gold     { 0xfff0b94a };
const juce::Colour cyan     { 0xff35d6e0 };
const juce::Colour text     { 0xffd8d6e6 };
const juce::Colour dim      { 0xff8a87a3 };
} // namespace ui

class ZoniumLnF : public juce::LookAndFeel_V4
{
public:
    ZoniumLnF()
    {
        setColour (juce::Slider::textBoxTextColourId, ui::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::backgroundColourId, ui::panel);
        setColour (juce::ComboBox::outlineColourId, ui::panelEdge);
        setColour (juce::ComboBox::textColourId, ui::text);
        setColour (juce::ComboBox::arrowColourId, ui::gold);
        setColour (juce::PopupMenu::backgroundColourId, ui::panel);
        setColour (juce::PopupMenu::textColourId, ui::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::gold.withAlpha (0.25f));
        setColour (juce::PopupMenu::highlightedTextColourId, ui::gold);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float ang = start + pos * (end - start);

        juce::Path track;
        track.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, start, end, true);
        g.setColour (juce::Colour (0xff2a2940));
        g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path val;
        val.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, start, ang, true);
        g.setGradientFill (juce::ColourGradient (ui::cyan, c.x - r, c.y, ui::gold, c.x + r, c.y, false));
        g.strokePath (val, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float kr = r - 9.0f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff45445c), c.x, c.y - kr, juce::Colour (0xff12121a), c.x, c.y + kr, false));
        g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
        g.setColour (juce::Colour (0xff5b5a78));
        g.drawEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f, 1.0f);

        juce::Path p;
        p.addRoundedRectangle (-1.5f, -kr + 3.0f, 3.0f, kr * 0.42f, 1.5f);
        g.setColour (ui::gold);
        g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool) override
    {
        auto r = b.getLocalBounds().toFloat().withSizeKeepingCentre (juce::jmax (10.0f, b.getWidth() - 12.0f), 28.0f);
        const bool on = b.getToggleState();
        g.setColour (on ? ui::gold.withAlpha (0.9f) : ui::panelEdge);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (on ? juce::Colour (0xff1a1305) : ui::dim);
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        g.drawText (b.getButtonText(), r.toNearestInt(), juce::Justification::centred);
    }
};

class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& name)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 14);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, ui::dim);
        label.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
        addAndMakeVisible (slider);
        addAndMakeVisible (label);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        label.setBounds (b.removeFromTop (16));
        slider.setBounds (b);
    }

private:
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class VoxMachineEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit VoxMachineEditor (VoxMachineProcessor&);
    ~VoxMachineEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }

private:
    void loadFile (const juce::File&);
    void timerCallback() override;
    void drawPanel (juce::Graphics&, juce::Rectangle<int>, const juce::String&);
    void drawMeter (juce::Graphics&, juce::Rectangle<float>, float level);
    void layoutRow (juce::Rectangle<int> panelRect, std::vector<juce::Component*> items);

    VoxMachineProcessor& proc;
    ZoniumLnF lnf;

    std::vector<std::unique_ptr<Knob>> vocK, carK, envK, outK;
    juce::ToggleButton monoBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> monoAtt;
    juce::ComboBox presetBox;
    juce::TextButton loadBtn;
    juce::ToggleButton loopBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> loopAtt;
    std::unique_ptr<juce::FileChooser> chooser;
    bool dragOver = false;
    float playPos = 0.0f;

    juce::Rectangle<int> displayRect, vocR, outR, carR, envR, bottomRect;
    float bands[32] {}, inLevel = 0.0f, outLevel = 0.0f, led = 0.0f;
    int voices = 0, nBands = 20;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxMachineEditor)
};
