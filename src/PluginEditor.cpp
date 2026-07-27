#include "PluginEditor.h"

namespace
{
const juce::Colour kBg { 0xff0a0a0f };
const juce::Colour kBone { 0xffd4c9a8 };
const juce::Colour kAccent { 0xffc8a44e };
const juce::Colour kDim { 0x88d4c9a8 };
const juce::Colour kCyan { 0xff64c8ff };

void styleKnob (juce::Slider& s, const juce::String& name)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 52, 13);
    s.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
    s.setColour (juce::Slider::rotarySliderOutlineColourId, kBone.withAlpha (0.25f));
    s.setColour (juce::Slider::thumbColourId, kBone);
    s.setColour (juce::Slider::textBoxTextColourId, kBone);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setName (name);
}

void styleLfo (juce::Slider& s, const juce::String& name)
{
    styleKnob (s, name);
    s.setColour (juce::Slider::rotarySliderFillColourId, kCyan);
}
} // namespace

FringeAudioProcessorEditor::FringeAudioProcessorEditor (FringeAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (980, 720);
    setResizable (true, true);
    setResizeLimits (800, 560, 1800, 1200);
    setWantsKeyboardFocus (true);

    titleLabel.setText ("FRINGE", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (20.0f).withStyle ("Bold"));
    titleLabel.setColour (juce::Label::textColourId, kBone);
    addAndMakeVisible (titleLabel);

    hintLabel.setText ("draw on field in Draw preset · dbl-click clears · space = source", juce::dontSendNotification);
    hintLabel.setFont (juce::FontOptions (11.0f));
    hintLabel.setColour (juce::Label::textColourId, kDim);
    addAndMakeVisible (hintLabel);

    presetBox.addItemList ({ "Single:Slit", "Double:Slit", "Lens", "Diffraction",
                             "Mach-Zehnder", "Draw", "Open Field" },
                           1);
    // cleaner labels
    presetBox.clear (juce::dontSendNotification);
    presetBox.addItemList ({ "Single Slit", "Double Slit", "Convex Lens", "Diffraction",
                             "Mach-Zehnder", "Draw", "Open Field" },
                           1);
    presetBox.setColour (juce::ComboBox::backgroundColourId, kBg.brighter (0.05f));
    presetBox.setColour (juce::ComboBox::textColourId, kBone);
    presetBox.setColour (juce::ComboBox::outlineColourId, kBone.withAlpha (0.2f));
    addAndMakeVisible (presetBox);

    auto styleBtn = [] (juce::TextButton& b, bool toggle = true) {
        b.setClickingTogglesState (toggle);
        b.setColour (juce::TextButton::buttonColourId, kBg.brighter (0.08f));
        b.setColour (juce::TextButton::buttonOnColourId, kAccent.withAlpha (0.35f));
        b.setColour (juce::TextButton::textColourOffId, kDim);
        b.setColour (juce::TextButton::textColourOnId, kBone);
    };
    styleBtn (gateButton);
    styleBtn (scaleButton);
    styleBtn (droneButton);
    styleBtn (eraserButton);
    styleBtn (clearDrawButton, false);
    addAndMakeVisible (gateButton);
    addAndMakeVisible (scaleButton);
    addAndMakeVisible (droneButton);
    addAndMakeVisible (eraserButton);
    addAndMakeVisible (clearDrawButton);

    clearDrawButton.onClick = [this] {
        audioProcessor.getEngine().clearDrawing();
    };
    eraserButton.onClick = [this] {
        eraser_ = eraserButton.getToggleState();
    };

    styleKnob (volumeSlider, "Vol");
    styleKnob (speedSlider, "Speed");
    styleKnob (freqSlider, "Freq");
    styleKnob (slitSlider, "Slit");
    styleKnob (slitWSlider, "Width");
    styleKnob (sensSlider, "Sens");
    styleKnob (filterSlider, "Filter");
    styleKnob (reverbSlider, "Reverb");
    styleLfo (lfo1Rate, "L1 Rate");
    styleLfo (lfo1Depth, "L1 Dep");
    styleLfo (lfo2Rate, "L2 Rate");
    styleLfo (lfo2Depth, "L2 Dep");
    styleLfo (lfo3Rate, "L3 Rate");
    styleLfo (lfo3Depth, "L3 Dep");

    for (auto* s : { &volumeSlider, &speedSlider, &freqSlider, &slitSlider, &slitWSlider,
                     &sensSlider, &filterSlider, &reverbSlider,
                     &lfo1Rate, &lfo1Depth, &lfo2Rate, &lfo2Depth, &lfo3Rate, &lfo3Depth })
        addAndMakeVisible (s);

    auto& ap = audioProcessor.getAPVTS();
    volumeAtt = std::make_unique<SA> (ap, "volume", volumeSlider);
    speedAtt = std::make_unique<SA> (ap, "speed", speedSlider);
    freqAtt = std::make_unique<SA> (ap, "freq", freqSlider);
    slitAtt = std::make_unique<SA> (ap, "slit", slitSlider);
    slitWAtt = std::make_unique<SA> (ap, "slitW", slitWSlider);
    sensAtt = std::make_unique<SA> (ap, "sens", sensSlider);
    filterAtt = std::make_unique<SA> (ap, "filter", filterSlider);
    reverbAtt = std::make_unique<SA> (ap, "reverb", reverbSlider);
    lfo1RateAtt = std::make_unique<SA> (ap, "lfo1Rate", lfo1Rate);
    lfo1DepthAtt = std::make_unique<SA> (ap, "lfo1Depth", lfo1Depth);
    lfo2RateAtt = std::make_unique<SA> (ap, "lfo2Rate", lfo2Rate);
    lfo2DepthAtt = std::make_unique<SA> (ap, "lfo2Depth", lfo2Depth);
    lfo3RateAtt = std::make_unique<SA> (ap, "lfo3Rate", lfo3Rate);
    lfo3DepthAtt = std::make_unique<SA> (ap, "lfo3Depth", lfo3Depth);
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

juce::Rectangle<int> FringeAudioProcessorEditor::fieldBounds() const
{
    return fieldArea_;
}

juce::Colour FringeAudioProcessorEditor::amplitudeColour (float amp, float uvX) const
{
    const float t = std::clamp (std::abs (amp) * 3.5f, 0.0f, 1.0f);
    const float age = std::clamp (uvX, 0.0f, 1.0f);

    const juce::Colour young = juce::Colour::fromFloatRGBA (0.15f, 0.62f, 0.72f, 1.0f);
    const juce::Colour mid = juce::Colour::fromFloatRGBA (0.92f, 0.72f, 0.28f, 1.0f);
    const juce::Colour old = juce::Colour::fromFloatRGBA (0.9f, 0.32f, 0.52f, 1.0f);

    juce::Colour base = age < 0.4f ? young.interpolatedWith (mid, age / 0.4f)
                                   : mid.interpolatedWith (old, (age - 0.4f) / 0.6f);
    if (amp < 0.0f)
        base = base.withRotatedHue (0.08f).darker (0.25f);

    // soft bloom: brighter peaks
    const float bloom = std::clamp ((t - 0.55f) * 2.2f, 0.0f, 1.0f);
    base = base.interpolatedWith (juce::Colours::white, bloom * 0.35f);

    const juce::Colour lo { 0xff04050a };
    return lo.interpolatedWith (base, std::pow (t, 0.85f));
}

void FringeAudioProcessorEditor::paintWaveField (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (kBg);
    g.fillRect (area);

    if (snapshot_.w <= 0 || snapshot_.amp.empty())
    {
        g.setColour (kDim);
        g.drawText ("wave field warming up…", area, juce::Justification::centred);
        return;
    }

    const int w = snapshot_.w;
    const int h = snapshot_.h;
    juce::Image img (juce::Image::RGB, w, h, false);

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            const size_t i = static_cast<size_t> (y * w + x);
            const float spd = snapshot_.speed[i];
            const float amp = snapshot_.amp[i];
            const float uvX = (static_cast<float> (x) + 0.5f) / static_cast<float> (w);

            juce::Colour c;
            if (spd < 0.05f)
                c = juce::Colour (0xff14101c);
            else if (spd < 0.7f)
                c = juce::Colour (0xff1a2830).interpolatedWith (amplitudeColour (amp, uvX), 0.55f); // BS/lens
            else
                c = amplitudeColour (amp, uvX);

            if (std::abs (uvX - snapshot_.detectorX) < 0.007f)
                c = c.interpolatedWith (kAccent, 0.55f);

            img.setPixelAt (x, h - 1 - y, c);
        }
    }

    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (img, area.toFloat());

    // source line
    const float sx = area.getX() + snapshot_.sourceX * (float) area.getWidth();
    g.setColour (kAccent.withAlpha (0.65f));
    g.drawLine (sx, (float) area.getY() + 4, sx, (float) area.getBottom() - 4, 1.2f);

    g.setColour (kBone.withAlpha (0.18f));
    g.drawRect (area, 1);

    if (audioProcessor.getEngine().isDrawPreset())
    {
        g.setColour (kAccent.withAlpha (0.8f));
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (eraser_ ? "DRAW MODE · eraser" : "DRAW MODE · paint walls",
                    area.reduced (8).removeFromTop (18), juce::Justification::topLeft);
    }
}

void FringeAudioProcessorEditor::paintDetectorGraph (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (kBg.brighter (0.04f));
    g.fillRoundedRectangle (area.toFloat(), 4.0f);
    g.setColour (kBone.withAlpha (0.15f));
    g.drawRoundedRectangle (area.toFloat(), 4.0f, 1.0f);

    g.setColour (kDim);
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("DETECTOR", area.reduced (6).removeFromTop (14), juce::Justification::centredLeft);

    if (snapshot_.detector.empty())
        return;

    juce::Path path;
    const auto plot = area.reduced (8, 18);
    const int n = static_cast<int> (snapshot_.detector.size());
    for (int i = 0; i < n; ++i)
    {
        const float v = std::tanh (std::abs (snapshot_.detector[static_cast<size_t> (i)]) * 2.0f);
        const float x = plot.getX() + (float) i / (float) std::max (1, n - 1) * plot.getWidth();
        const float y = plot.getBottom() - v * plot.getHeight();
        if (i == 0)
            path.startNewSubPath (x, y);
        else
            path.lineTo (x, y);
    }
    g.setColour (kAccent.withAlpha (0.9f));
    g.strokePath (path, juce::PathStrokeType (1.4f));

    g.setColour (kDim);
    g.drawText (juce::String (snapshot_.energy, 3), area.reduced (6).removeFromBottom (14),
                juce::Justification::centredRight);
}

void FringeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    auto bounds = getLocalBounds();
    auto top = bounds.removeFromTop (40);
    juce::ignoreUnused (top);

    auto bottom = bounds.removeFromBottom (200);
    auto side = bounds.removeFromRight (160).reduced (8, 4);
    fieldArea_ = bounds.reduced (12, 4);

    paintWaveField (g, fieldArea_);
    paintDetectorGraph (g, side);

    g.setColour (kBone.withAlpha (0.12f));
    g.drawLine ((float) bottom.getX(), (float) bottom.getY(), (float) bottom.getRight(), (float) bottom.getY());

    g.setColour (kDim);
    g.setFont (juce::FontOptions (9.0f));
    for (auto* s : { &volumeSlider, &speedSlider, &freqSlider, &slitSlider, &slitWSlider,
                     &sensSlider, &filterSlider, &reverbSlider,
                     &lfo1Rate, &lfo1Depth, &lfo2Rate, &lfo2Depth, &lfo3Rate, &lfo3Depth })
    {
        auto r = s->getBounds();
        g.drawText (s->getName(), r.getX(), r.getBottom() - 2, r.getWidth(), 12, juce::Justification::centred);
    }
}

void FringeAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto top = r.removeFromTop (32);
    titleLabel.setBounds (top.removeFromLeft (100));
    hintLabel.setBounds (top.removeFromLeft (340));
    clearDrawButton.setBounds (top.removeFromRight (64).reduced (2));
    eraserButton.setBounds (top.removeFromRight (64).reduced (2));
    droneButton.setBounds (top.removeFromRight (64).reduced (2));
    scaleButton.setBounds (top.removeFromRight (64).reduced (2));
    gateButton.setBounds (top.removeFromRight (72).reduced (2));
    presetBox.setBounds (top.removeFromRight (150).reduced (2));

    auto bottom = r.removeFromBottom (200);
    bottom.removeFromTop (6);
    bottom.removeFromBottom (14);

    // row1 knobs, row2 lfos
    auto row1 = bottom.removeFromTop (bottom.getHeight() / 2);
    auto row2 = bottom;
    juce::Slider* row1s[] = { &volumeSlider, &speedSlider, &freqSlider, &slitSlider, &slitWSlider,
                              &sensSlider, &filterSlider, &reverbSlider };
    juce::Slider* row2s[] = { &lfo1Rate, &lfo1Depth, &lfo2Rate, &lfo2Depth, &lfo3Rate, &lfo3Depth };

    const int n1 = 8;
    const int cell1 = row1.getWidth() / n1;
    for (int i = 0; i < n1; ++i)
        row1s[i]->setBounds (row1.withX (row1.getX() + i * cell1).withWidth (cell1).reduced (6, 2));

    const int n2 = 6;
    const int cell2 = row2.getWidth() / n2;
    for (int i = 0; i < n2; ++i)
        row2s[i]->setBounds (row2.withX (row2.getX() + i * cell2).withWidth (cell2).reduced (10, 2));
}

void FringeAudioProcessorEditor::paintAtEvent (const juce::MouseEvent& e)
{
    if (! fieldArea_.contains (e.getPosition()))
        return;
    if (! audioProcessor.getEngine().isDrawPreset())
        return;

    const float uvX = (float) (e.x - fieldArea_.getX()) / (float) fieldArea_.getWidth();
    const float uvY = 1.0f - (float) (e.y - fieldArea_.getY()) / (float) fieldArea_.getHeight();
    float brush = 0.01f;
    if (auto* v = audioProcessor.getAPVTS().getRawParameterValue ("slitW"))
        brush = std::clamp (v->load(), 0.004f, 0.06f);

    audioProcessor.getEngine().paintAt (std::clamp (uvX, 0.0f, 1.0f),
                                        std::clamp (uvY, 0.0f, 1.0f),
                                        brush, eraser_);
}

void FringeAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    drawing_ = fieldArea_.contains (e.getPosition()) && audioProcessor.getEngine().isDrawPreset();
    if (drawing_)
        paintAtEvent (e);
}

void FringeAudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (drawing_)
        paintAtEvent (e);
}

void FringeAudioProcessorEditor::mouseUp (const juce::MouseEvent&)
{
    drawing_ = false;
}

void FringeAudioProcessorEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (fieldArea_.contains (e.getPosition()) && audioProcessor.getEngine().isDrawPreset())
        audioProcessor.getEngine().clearDrawing();
}

bool FringeAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
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
            audioProcessor.getEngine().noteOn (m.note, 0.9f);
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
