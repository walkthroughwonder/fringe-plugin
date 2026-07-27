#include "PluginEditor.h"

namespace
{
const juce::Colour kBg { 0xff0a0a0f };
const juce::Colour kBone { 0xffd4c9a8 };
const juce::Colour kAccent { 0xffc8a44e };
const juce::Colour kDim { 0x88d4c9a8 };

void styleKnob (juce::Slider& s, const juce::String& name)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 14);
    s.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
    s.setColour (juce::Slider::rotarySliderOutlineColourId, kBone.withAlpha (0.25f));
    s.setColour (juce::Slider::thumbColourId, kBone);
    s.setColour (juce::Slider::textBoxTextColourId, kBone);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setName (name);
}
} // namespace

FringeAudioProcessorEditor::FringeAudioProcessorEditor (FringeAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (900, 620);
    setResizable (true, true);
    setResizeLimits (720, 480, 1600, 1000);
    setWantsKeyboardFocus (true);

    titleLabel.setText ("FRINGE  ·  photonic synthesizer", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (18.0f));
    titleLabel.setColour (juce::Label::textColourId, kBone);
    addAndMakeVisible (titleLabel);

    presetBox.addItemList ({ "Single Slit", "Double Slit", "Convex Lens", "Diffraction", "Open Field" }, 1);
    presetBox.setColour (juce::ComboBox::backgroundColourId, kBg.brighter (0.05f));
    presetBox.setColour (juce::ComboBox::textColourId, kBone);
    presetBox.setColour (juce::ComboBox::outlineColourId, kBone.withAlpha (0.2f));
    addAndMakeVisible (presetBox);

    auto styleBtn = [] (juce::TextButton& b) {
        b.setClickingTogglesState (true);
        b.setColour (juce::TextButton::buttonColourId, kBg.brighter (0.08f));
        b.setColour (juce::TextButton::buttonOnColourId, kAccent.withAlpha (0.35f));
        b.setColour (juce::TextButton::textColourOffId, kDim);
        b.setColour (juce::TextButton::textColourOnId, kBone);
    };
    styleBtn (gateButton);
    styleBtn (scaleButton);
    styleBtn (droneButton);
    addAndMakeVisible (gateButton);
    addAndMakeVisible (scaleButton);
    addAndMakeVisible (droneButton);

    styleKnob (volumeSlider, "Vol");
    styleKnob (speedSlider, "Speed");
    styleKnob (freqSlider, "Freq");
    styleKnob (slitSlider, "Slit");
    styleKnob (sensSlider, "Sens");
    styleKnob (filterSlider, "Filter");
    styleKnob (reverbSlider, "Reverb");

    for (auto* s : { &volumeSlider, &speedSlider, &freqSlider, &slitSlider, &sensSlider, &filterSlider, &reverbSlider })
        addAndMakeVisible (s);

    auto& ap = audioProcessor.getAPVTS();
    volumeAtt = std::make_unique<SA> (ap, "volume", volumeSlider);
    speedAtt = std::make_unique<SA> (ap, "speed", speedSlider);
    freqAtt = std::make_unique<SA> (ap, "freq", freqSlider);
    slitAtt = std::make_unique<SA> (ap, "slit", slitSlider);
    sensAtt = std::make_unique<SA> (ap, "sens", sensSlider);
    filterAtt = std::make_unique<SA> (ap, "filter", filterSlider);
    reverbAtt = std::make_unique<SA> (ap, "reverb", reverbSlider);
    presetAtt = std::make_unique<CA> (ap, "preset", presetBox);
    gateAtt = std::make_unique<BA> (ap, "gate", gateButton);
    scaleAtt = std::make_unique<BA> (ap, "scaleMode", scaleButton);
    droneAtt = std::make_unique<BA> (ap, "droneMode", droneButton);

    startTimerHz (30);
}

FringeAudioProcessorEditor::~FringeAudioProcessorEditor()
{
    stopTimer();
}

void FringeAudioProcessorEditor::timerCallback()
{
    fringe::FieldSnapshot snap;
    if (audioProcessor.getEngine().pullSnapshot (snap))
        snapshot_ = std::move (snap);
    repaint();
}

juce::Colour FringeAudioProcessorEditor::amplitudeColour (float amp, float uvX) const
{
    // Cathedral age palette (simplified from wave-engine.js render shader)
    const float t = std::clamp (std::abs (amp) * 4.0f, 0.0f, 1.0f);
    const float age = std::clamp (uvX, 0.0f, 1.0f); // young left → old right

    juce::Colour young = juce::Colour::fromFloatRGBA (0.12f, 0.55f, 0.65f, 1.0f);
    juce::Colour mid = juce::Colour::fromFloatRGBA (0.85f, 0.65f, 0.22f, 1.0f);
    juce::Colour old = juce::Colour::fromFloatRGBA (0.85f, 0.30f, 0.50f, 1.0f);

    juce::Colour base = age < 0.45f ? young.interpolatedWith (mid, age / 0.45f)
                                    : mid.interpolatedWith (old, (age - 0.45f) / 0.55f);

    if (amp < 0.0f)
        base = base.darker (0.35f);

    const juce::Colour lo { 0xff05060a };
    return lo.interpolatedWith (base, t);
}

void FringeAudioProcessorEditor::paintWaveField (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (kBg);
    g.fillRect (area);

    if (snapshot_.w <= 0 || snapshot_.h <= 0 || snapshot_.amp.empty())
    {
        g.setColour (kDim);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText ("starting wave field… play notes or leave SOURCE on",
                    area, juce::Justification::centred);
        return;
    }

    const int w = snapshot_.w;
    const int h = snapshot_.h;
    juce::Image img (juce::Image::RGB, w, h, false);

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            const int i = y * w + x;
            const float spd = snapshot_.speed[static_cast<size_t> (i)];
            const float amp = snapshot_.amp[static_cast<size_t> (i)];
            const float uvX = (static_cast<float> (x) + 0.5f) / static_cast<float> (w);

            juce::Colour c;
            if (spd < 0.05f)
                c = juce::Colour (0xff1a1520); // wall
            else
                c = amplitudeColour (amp, uvX);

            // Detector line
            if (std::abs (uvX - snapshot_.detectorX) < 0.008f)
                c = c.brighter (0.4f).interpolatedWith (kAccent, 0.5f);

            img.setPixelAt (x, h - 1 - y, c); // flip Y for display
        }
    }

    g.drawImage (img, area.toFloat(), juce::RectanglePlacement::stretchToFit);

    // Source marker
    const float sx = area.getX() + snapshot_.sourceX * area.getWidth();
    g.setColour (kAccent.withAlpha (0.7f));
    g.drawLine (sx, (float) area.getY(), sx, (float) area.getBottom(), 1.5f);

    g.setColour (kBone.withAlpha (0.15f));
    g.drawRect (area, 1);
}

void FringeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    auto bounds = getLocalBounds();
    auto top = bounds.removeFromTop (36);
    // title drawn via label

    auto bottom = bounds.removeFromBottom (150);
    auto field = bounds.reduced (12, 4);

    paintWaveField (g, field);

    g.setColour (kBone.withAlpha (0.2f));
    g.drawLine ((float) bottom.getX(), (float) bottom.getY(), (float) bottom.getRight(), (float) bottom.getY(), 1.0f);

    // Knob labels
    g.setColour (kDim);
    g.setFont (juce::FontOptions (10.0f));
    for (auto* s : { &volumeSlider, &speedSlider, &freqSlider, &slitSlider, &sensSlider, &filterSlider, &reverbSlider })
    {
        auto r = s->getBounds();
        g.drawText (s->getName(), r.withY (r.getBottom() - 2).withHeight (14), juce::Justification::centred);
    }

    g.setColour (kDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("MIDI notes pulse the field  ·  QWERTY: Z-M / A-L / Q-P pentatonic  ·  match web Fringe",
                bottom.removeFromBottom (22).reduced (12, 0), juce::Justification::centredLeft);

    juce::ignoreUnused (top);
}

void FringeAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto top = r.removeFromTop (28);
    titleLabel.setBounds (top.removeFromLeft (320));
    droneButton.setBounds (top.removeFromRight (70).reduced (2, 2));
    scaleButton.setBounds (top.removeFromRight (70).reduced (2, 2));
    gateButton.setBounds (top.removeFromRight (80).reduced (2, 2));
    presetBox.setBounds (top.removeFromRight (160).reduced (2, 2));

    auto bottom = r.removeFromBottom (150);
    bottom.removeFromTop (8);
    bottom.removeFromBottom (24);

    const int n = 7;
    const int cell = bottom.getWidth() / n;
    juce::Slider* knobs[] = { &volumeSlider, &speedSlider, &freqSlider, &slitSlider,
                              &sensSlider, &filterSlider, &reverbSlider };
    for (int i = 0; i < n; ++i)
    {
        auto cellR = bottom.withX (bottom.getX() + i * cell).withWidth (cell).reduced (8, 4);
        knobs[i]->setBounds (cellR);
    }
}

bool FringeAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    // QWERTY pentatonic rows (approx web map) → MIDI note-ons
    struct Map { int key; int note; };
    static const Map maps[] = {
        { 'z', 48 }, { 'x', 50 }, { 'c', 52 }, { 'v', 55 }, { 'b', 57 },
        { 'n', 60 }, { 'm', 62 },
        { 'a', 60 }, { 's', 62 }, { 'd', 64 }, { 'f', 67 }, { 'g', 69 },
        { 'h', 72 }, { 'j', 74 }, { 'k', 76 }, { 'l', 79 },
        { 'q', 72 }, { 'w', 74 }, { 'e', 76 }, { 'r', 79 }, { 't', 81 },
        { 'y', 84 }, { 'u', 86 }, { 'i', 88 }, { 'o', 91 }, { 'p', 93 },
    };

    const auto ch = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());
    for (const auto& m : maps)
    {
        if (ch == m.key)
        {
            audioProcessor.getEngine().noteOn (m.note, 0.85f);
            return true;
        }
    }

    if (key == juce::KeyPress::spaceKey)
    {
        if (auto* p = audioProcessor.getAPVTS().getParameter ("gate"))
            p->setValueNotifyingHost (p->getValue() < 0.5f ? 1.0f : 0.0f);
        return true;
    }

    return false;
}
