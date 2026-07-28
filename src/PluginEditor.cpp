#include "PluginEditor.h"

namespace
{
// Cinematic dark palette
const juce::Colour kVoid   { 0xff06060a };
const juce::Colour kPanel  { 0xff0c0c12 };
const juce::Colour kRail   { 0xff0a0a10 };
const juce::Colour kBone   { 0xffe4d8b8 };
const juce::Colour kMuted  { 0x66e4d8b8 };
const juce::Colour kGold   { 0xffc9a84c };
const juce::Colour kGoldDim{ 0x55c9a84c };
const juce::Colour kCyan   { 0xff5ec8e8 };
const juce::Colour kRose   { 0xffd06090 };
const juce::Colour kLine   { 0x22ffffff };

constexpr double kAspect = 20.0 / 9.0; // cinematic ultra-wide
} // namespace

void FringeAudioProcessorEditor::styleKnob (juce::Slider& s, const juce::String& name, juce::Colour accent)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 48, 12);
    s.setColour (juce::Slider::rotarySliderFillColourId, accent);
    s.setColour (juce::Slider::rotarySliderOutlineColourId, kBone.withAlpha (0.18f));
    s.setColour (juce::Slider::thumbColourId, kBone.withAlpha (0.9f));
    s.setColour (juce::Slider::textBoxTextColourId, kMuted);
    s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setName (name);
    s.setTooltip (name);
}

void FringeAudioProcessorEditor::styleToggle (juce::TextButton& b)
{
    b.setClickingTogglesState (true);
    b.setColour (juce::TextButton::buttonColourId, kPanel.brighter (0.04f));
    b.setColour (juce::TextButton::buttonOnColourId, kGold.withAlpha (0.28f));
    b.setColour (juce::TextButton::textColourOffId, kMuted);
    b.setColour (juce::TextButton::textColourOnId, kBone);
    b.setColour (juce::ComboBox::outlineColourId, kLine);
}

FringeAudioProcessorEditor::FringeAudioProcessorEditor (FringeAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // 20:9 default — 1280×576
    constrainer_.setFixedAspectRatio (kAspect);
    constrainer_.setSizeLimits (1000, 450, 2400, 1080);
    setConstrainer (&constrainer_);
    setResizable (true, true);
    setSize (1280, 576);
    setWantsKeyboardFocus (true);

    brandLabel.setText ("FRINGE", juce::dontSendNotification);
    brandLabel.setFont (juce::FontOptions (22.0f).withStyle ("Bold"));
    brandLabel.setColour (juce::Label::textColourId, kBone);
    brandLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (brandLabel);

    tagLabel.setText ("photonic synthesizer", juce::dontSendNotification);
    tagLabel.setFont (juce::FontOptions (11.0f));
    tagLabel.setColour (juce::Label::textColourId, kGold.withAlpha (0.75f));
    tagLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (tagLabel);

    presetBox.clear (juce::dontSendNotification);
    presetBox.addItemList ({ "Single Slit", "Double Slit", "Convex Lens", "Diffraction",
                             "Mach-Zehnder", "Draw", "Open Field" },
                           1);
    presetBox.setColour (juce::ComboBox::backgroundColourId, kPanel);
    presetBox.setColour (juce::ComboBox::textColourId, kBone);
    presetBox.setColour (juce::ComboBox::outlineColourId, kGoldDim);
    presetBox.setColour (juce::ComboBox::arrowColourId, kGold);
    presetBox.setTooltip ("Optical preset");
    addAndMakeVisible (presetBox);

    styleToggle (gateButton);
    styleToggle (scaleButton);
    styleToggle (droneButton);
    styleToggle (eraserButton);
    clearDrawButton.setClickingTogglesState (false);
    clearDrawButton.setColour (juce::TextButton::buttonColourId, kPanel.brighter (0.04f));
    clearDrawButton.setColour (juce::TextButton::textColourOffId, kMuted);

    for (auto* b : { &gateButton, &scaleButton, &droneButton, &eraserButton, &clearDrawButton })
        addAndMakeVisible (b);

    clearDrawButton.onClick = [this] { audioProcessor.getEngine().clearDrawing(); fieldDirty_ = true; };
    eraserButton.onClick = [this] { eraser_ = eraserButton.getToggleState(); };

    styleKnob (volumeSlider, "VOL", kGold);
    styleKnob (speedSlider, "SPEED", kGold);
    styleKnob (freqSlider, "FREQ", kGold);
    styleKnob (slitSlider, "SLIT", kGold);
    styleKnob (slitWSlider, "WIDTH", kGold);
    styleKnob (sensSlider, "SENS", kGold);
    styleKnob (filterSlider, "FILTER", kGold);
    styleKnob (reverbSlider, "VERB", kGold);
    styleKnob (lfo1Rate, "L1 RATE", kCyan);
    styleKnob (lfo1Depth, "L1 DEP", kCyan);
    styleKnob (lfo2Rate, "L2 RATE", kCyan);
    styleKnob (lfo2Depth, "L2 DEP", kCyan);
    styleKnob (lfo3Rate, "L3 RATE", kCyan);
    styleKnob (lfo3Depth, "L3 DEP", kCyan);

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
    {
        snapshot_ = std::move (snap);
        fieldDirty_ = true;
    }
    animPhase_ += 0.04f;
    if (animPhase_ > 1000.0f)
        animPhase_ = 0.0f;
    repaint();
}

juce::Colour FringeAudioProcessorEditor::amplitudeColour (float amp, float uvX, float energy) const
{
    // High-contrast fringes: amplitude drives brightness hard so interference reads as stripes
    const float mag = std::abs (amp);
    // Auto-ish gain: quiet fields still show structure
    const float gain = 6.5f + energy * 4.0f;
    const float t = std::clamp (mag * gain, 0.0f, 1.0f);
    // gamma for punchy peaks without washing mids
    const float tg = std::pow (t, 0.72f);
    const float age = std::clamp (uvX, 0.0f, 1.0f);

    const juce::Colour young (0xff3ec8d8); // cyan
    const juce::Colour mid (0xffe0b040);   // amber
    const juce::Colour old (0xffe07098);   // rose
    juce::Colour hue = age < 0.4f ? young.interpolatedWith (mid, age / 0.4f)
                                  : mid.interpolatedWith (old, (age - 0.4f) / 0.6f);

    // polarity: negative phase slightly cooler/darker (classic interference look)
    if (amp < 0.0f)
        hue = hue.withRotatedHue (0.08f).darker (0.35f);

    // peak sparkle only at true highs (not a mushy bloom wash)
    if (tg > 0.75f)
        hue = hue.interpolatedWith (juce::Colours::white, (tg - 0.75f) * 1.2f);

    const juce::Colour deep (0xff05060c);
    // floor dim so zero-crossings read black (fringe lines)
    return deep.interpolatedWith (hue, tg);
}

juce::Image FringeAudioProcessorEditor::renderFieldImage() const
{
    if (snapshot_.w <= 0 || snapshot_.amp.empty())
        return {};

    const int w = snapshot_.w;
    const int h = snapshot_.h;
    juce::Image img (juce::Image::ARGB, w, h, true);
    const float energy = snapshot_.energy;

    // percentile-ish scale: find max abs amp for adaptive contrast
    float maxA = 1.0e-5f;
    for (float a : snapshot_.amp)
        maxA = std::max (maxA, std::abs (a));
    const float invMax = 1.0f / maxA;

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            const size_t i = static_cast<size_t> (y * w + x);
            const float spd = snapshot_.speed[i];
            const float amp = snapshot_.amp[i] * invMax; // normalize field
            const float uvX = (static_cast<float> (x) + 0.5f) / static_cast<float> (w);

            juce::Colour c;
            if (spd < 0.05f)
            {
                // solid wall — high contrast vs field
                c = juce::Colour (0xff0a0810);
                // bright edge if free-space neighbor (reads as drawn barrier)
                bool edge = false;
                if (x > 0 && snapshot_.speed[i - 1] > 0.2f) edge = true;
                if (x + 1 < w && snapshot_.speed[i + 1] > 0.2f) edge = true;
                if (y > 0 && snapshot_.speed[i - (size_t) w] > 0.2f) edge = true;
                if (y + 1 < h && snapshot_.speed[i + (size_t) w] > 0.2f) edge = true;
                if (edge)
                    c = juce::Colour (0xffc9a84c).withAlpha (0.85f).interpolatedWith (c, 0.35f);
            }
            else if (spd < 0.72f)
            {
                // beamsplitter / lens medium
                c = amplitudeColour (amp, uvX, energy).interpolatedWith (juce::Colour (0xff1a3040), 0.25f);
            }
            else
            {
                c = amplitudeColour (amp, uvX, energy);
            }

            // thin detector hairline (not a fat glow wash)
            if (std::abs (uvX - snapshot_.detectorX) < (1.5f / (float) w))
                c = c.interpolatedWith (kGold, 0.65f);

            img.setPixelAt (x, h - 1 - y, c);
        }
    }

    // NO full-field blur — keep interference stripes sharp
    return img;
}

void FringeAudioProcessorEditor::paintChrome (juce::Graphics& g)
{
    g.fillAll (kVoid);

    // subtle top vignette gradient
    juce::ColourGradient grad (kPanel, 0, 0, kVoid, 0, (float) getHeight() * 0.35f, false);
    g.setGradientFill (grad);
    g.fillRect (0, 0, getWidth(), getHeight() / 3);

    // outer gold hairline
    g.setColour (kGold.withAlpha (0.22f));
    g.drawRect (getLocalBounds().reduced (1), 1);
    g.setColour (kLine);
    g.drawRect (getLocalBounds().reduced (3), 1);
}

void FringeAudioProcessorEditor::paintWaveField (juce::Graphics& g, juce::Rectangle<int> area)
{
    // panel behind field
    g.setColour (kPanel);
    g.fillRoundedRectangle (area.toFloat().expanded (2.0f), 6.0f);

    if (fieldDirty_ || fieldCache_.isNull())
    {
        fieldCache_ = renderFieldImage();
        fieldDirty_ = false;
    }

    auto inner = area.reduced (2);

    if (fieldCache_.isNull())
    {
        g.setColour (kMuted);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText ("initializing wave field...", inner, juce::Justification::centred);
    }
    else
    {
        // medium quality keeps fringe detail when upscaling low-res sim
        g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
        g.drawImage (fieldCache_, inner.toFloat());

        // light vignette only (was washing out fringes)
        juce::ColourGradient vig (juce::Colours::transparentBlack, (float) inner.getCentreX(), (float) inner.getCentreY(),
                                  juce::Colours::black.withAlpha (0.22f), (float) inner.getX(), (float) inner.getY(), true);
        g.setGradientFill (vig);
        g.fillRect (inner);

        // source marker
        if (snapshot_.w > 0)
        {
            const float sx = inner.getX() + snapshot_.sourceX * (float) inner.getWidth();
            g.setColour (kGold.withAlpha (0.35f + 0.15f * std::sin (animPhase_)));
            g.drawLine (sx, (float) inner.getY() + 6, sx, (float) inner.getBottom() - 6, 1.2f);
            g.setFont (juce::FontOptions (9.0f));
            g.setColour (kGold.withAlpha (0.7f));
            g.drawText ("SRC", juce::Rectangle<float> (sx - 14, (float) inner.getY() + 4, 28, 12),
                        juce::Justification::centred);
        }

        // detector label
        if (snapshot_.w > 0)
        {
            const float dx = inner.getX() + snapshot_.detectorX * (float) inner.getWidth();
            g.setColour (kGold.withAlpha (0.5f));
            g.setFont (juce::FontOptions (9.0f));
            g.drawText ("DET", juce::Rectangle<float> (dx - 14, (float) inner.getBottom() - 16, 28, 12),
                        juce::Justification::centred);
        }
    }

    // draw-mode cursor
    if (audioProcessor.getEngine().isDrawPreset() && hoverUv_.x >= 0.0f)
    {
        const float px = inner.getX() + hoverUv_.x * (float) inner.getWidth();
        const float py = inner.getY() + (1.0f - hoverUv_.y) * (float) inner.getHeight();
        float brush = 0.012f;
        if (auto* v = audioProcessor.getAPVTS().getRawParameterValue ("slitW"))
            brush = v->load();
        const float r = brush * (float) juce::jmin (inner.getWidth(), inner.getHeight());
        g.setColour (eraser_ ? kCyan.withAlpha (0.45f) : kGold.withAlpha (0.45f));
        g.drawEllipse (px - r, py - r, r * 2, r * 2, 1.2f);
    }

    // frame
    g.setColour (kGold.withAlpha (0.28f));
    g.drawRoundedRectangle (area.toFloat(), 4.0f, 1.0f);

    if (audioProcessor.getEngine().isDrawPreset())
    {
        g.setColour (kGold.withAlpha (0.85f));
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (eraser_ ? "DRAW mode: eraser" : "DRAW mode: paint walls  |  double-click clears",
                    area.reduced (10).removeFromTop (16), juce::Justification::topLeft);
    }
}

void FringeAudioProcessorEditor::paintDetectorPanel (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (kPanel);
    g.fillRoundedRectangle (area.toFloat(), 6.0f);
    g.setColour (kGold.withAlpha (0.2f));
    g.drawRoundedRectangle (area.toFloat(), 6.0f, 1.0f);

    auto r = area.reduced (10, 8);
    g.setColour (kMuted);
    g.setFont (juce::FontOptions (9.0f));
    g.drawText ("DETECTOR", r.removeFromTop (14), juce::Justification::centredLeft);

    // energy meter
    auto meter = r.removeFromTop (8);
    g.setColour (kVoid);
    g.fillRoundedRectangle (meter.toFloat(), 2.0f);
    const float e = std::clamp (snapshot_.energy * 8.0f, 0.0f, 1.0f);
    g.setColour (kGold.interpolatedWith (kCyan, e));
    g.fillRoundedRectangle (meter.toFloat().withWidth (meter.getWidth() * e), 2.0f);

    r.removeFromTop (6);
    g.setColour (kMuted);
    g.drawText ("energy  " + juce::String (snapshot_.energy, 3), r.removeFromTop (12), juce::Justification::centredLeft);

    r.removeFromTop (4);
    auto plot = r.removeFromTop (r.getHeight() - 28);
    g.setColour (kVoid);
    g.fillRoundedRectangle (plot.toFloat(), 3.0f);

    if (! snapshot_.detector.empty())
    {
        juce::Path path;
        const int n = static_cast<int> (snapshot_.detector.size());
        for (int i = 0; i < n; ++i)
        {
            const float v = std::tanh (std::abs (snapshot_.detector[static_cast<size_t> (i)]) * 1.6f);
            const float x = plot.getX() + 4 + (float) i / (float) std::max (1, n - 1) * (plot.getWidth() - 8);
            const float y = plot.getBottom() - 4 - v * (plot.getHeight() - 8);
            if (i == 0)
                path.startNewSubPath (x, y);
            else
                path.lineTo (x, y);
        }

        // fill under curve
        juce::Path fill = path;
        fill.lineTo ((float) plot.getRight() - 4, (float) plot.getBottom() - 4);
        fill.lineTo ((float) plot.getX() + 4, (float) plot.getBottom() - 4);
        fill.closeSubPath();
        g.setColour (kGold.withAlpha (0.12f));
        g.fillPath (fill);

        g.setColour (kGold.withAlpha (0.9f));
        g.strokePath (path, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved));
    }

    r.removeFromTop (6);
    g.setColour (kMuted);
    g.setFont (juce::FontOptions (9.0f));
    g.drawText ("MIDI / QWERTY / SPACE", r, juce::Justification::centredLeft);
}

void FringeAudioProcessorEditor::paintKnobRailLabels (juce::Graphics& g)
{
    g.setFont (juce::FontOptions (8.5f));
    g.setColour (kMuted);
    for (auto* s : { &volumeSlider, &speedSlider, &freqSlider, &slitSlider, &slitWSlider,
                     &sensSlider, &filterSlider, &reverbSlider,
                     &lfo1Rate, &lfo1Depth, &lfo2Rate, &lfo2Depth, &lfo3Rate, &lfo3Depth })
    {
        auto b = s->getBounds();
        g.drawText (s->getName(), b.getX(), b.getBottom() - 1, b.getWidth(), 11, juce::Justification::centred);
    }

    // section captions sit just above the knob rail (not over the wave field)
    if (! knobRail_.isEmpty())
    {
        g.setColour (kGold.withAlpha (0.55f));
        g.setFont (juce::FontOptions (9.0f));
        // 8 tone/field knobs then 6 LFO: split 8/6
        const int cell = knobRail_.getWidth() / 14;
        g.drawText ("TONE", knobRail_.getX(), knobRail_.getY() - 13, cell * 4, 12, juce::Justification::centred);
        g.drawText ("FIELD", knobRail_.getX() + cell * 4, knobRail_.getY() - 13, cell * 4, 12, juce::Justification::centred);
        g.drawText ("MOD", knobRail_.getX() + cell * 8, knobRail_.getY() - 13, cell * 6, 12, juce::Justification::centred);
    }
}

void FringeAudioProcessorEditor::paint (juce::Graphics& g)
{
    paintChrome (g);

    if (! fieldArea_.isEmpty())
        paintWaveField (g, fieldArea_);
    if (! detectorArea_.isEmpty())
        paintDetectorPanel (g, detectorArea_);

    // knob rail background
    if (! knobRail_.isEmpty())
    {
        g.setColour (kRail);
        g.fillRoundedRectangle (knobRail_.toFloat().expanded (6.0f, 4.0f), 8.0f);
        g.setColour (kLine);
        g.drawRoundedRectangle (knobRail_.toFloat().expanded (6.0f, 4.0f), 8.0f, 1.0f);
    }

    paintKnobRailLabels (g);

    // status bar
    if (! statusBar_.isEmpty())
    {
        g.setColour (kMuted);
        g.setFont (juce::FontOptions (10.0f));
        juce::String status = "20:9  |  mid/bass factory  |  ";
        status += gateButton.getToggleState() ? "source on  |  " : "source off  |  ";
        status += scaleButton.getToggleState() ? "scale  |  " : "";
        status += droneButton.getToggleState() ? "drone  |  " : "";
        status += "keys Z-P  |  space = source";
        g.drawText (status, statusBar_, juce::Justification::centredLeft);
    }
}

void FringeAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (12, 10);

    topBar_ = r.removeFromTop (34);
    {
        auto t = topBar_;
        brandLabel.setBounds (t.removeFromLeft (100));
        tagLabel.setBounds (t.removeFromLeft (160));
        clearDrawButton.setBounds (t.removeFromRight (60).reduced (2, 4));
        eraserButton.setBounds (t.removeFromRight (60).reduced (2, 4));
        droneButton.setBounds (t.removeFromRight (64).reduced (2, 4));
        scaleButton.setBounds (t.removeFromRight (64).reduced (2, 4));
        gateButton.setBounds (t.removeFromRight (72).reduced (2, 4));
        presetBox.setBounds (t.removeFromRight (150).reduced (2, 4));
    }

    statusBar_ = r.removeFromBottom (18);
    r.removeFromBottom (2);

    // space for section labels above knobs
    r.removeFromBottom (14);
    // bottom knob rail ~26% of remaining height
    const int railH = juce::jlimit (100, 128, r.getHeight() * 26 / 100);
    knobRail_ = r.removeFromBottom (railH);
    r.removeFromBottom (8);

    // right detector strip ~14% width
    const int detW = juce::jlimit (120, 170, r.getWidth() * 14 / 100);
    detectorArea_ = r.removeFromRight (detW);
    r.removeFromRight (10);

    fieldArea_ = r; // main cinematic stage

    // knobs: 8 tone/field + 6 LFO across rail
    juce::Slider* knobs[] = {
        &volumeSlider, &speedSlider, &freqSlider, &filterSlider,
        &slitSlider, &slitWSlider, &sensSlider, &reverbSlider,
        &lfo1Rate, &lfo1Depth, &lfo2Rate, &lfo2Depth, &lfo3Rate, &lfo3Depth
    };
    const int n = 14;
    auto rail = knobRail_.reduced (4, 6);
    const int cell = rail.getWidth() / n;
    for (int i = 0; i < n; ++i)
        knobs[i]->setBounds (rail.getX() + i * cell, rail.getY(), cell, rail.getHeight() - 10);
}

void FringeAudioProcessorEditor::paintAtEvent (const juce::MouseEvent& e)
{
    if (! fieldArea_.contains (e.getPosition()))
        return;
    if (! audioProcessor.getEngine().isDrawPreset())
        return;

    const float uvX = (float) (e.x - fieldArea_.getX()) / (float) juce::jmax (1, fieldArea_.getWidth());
    const float uvY = 1.0f - (float) (e.y - fieldArea_.getY()) / (float) juce::jmax (1, fieldArea_.getHeight());
    float brush = 0.012f;
    if (auto* v = audioProcessor.getAPVTS().getRawParameterValue ("slitW"))
        brush = std::clamp (v->load(), 0.004f, 0.06f);

    audioProcessor.getEngine().paintAt (std::clamp (uvX, 0.0f, 1.0f),
                                        std::clamp (uvY, 0.0f, 1.0f),
                                        brush, eraser_);
    fieldDirty_ = true;
}

void FringeAudioProcessorEditor::mouseMove (const juce::MouseEvent& e)
{
    if (fieldArea_.contains (e.getPosition()))
    {
        hoverUv_ = {
            (float) (e.x - fieldArea_.getX()) / (float) juce::jmax (1, fieldArea_.getWidth()),
            1.0f - (float) (e.y - fieldArea_.getY()) / (float) juce::jmax (1, fieldArea_.getHeight())
        };
    }
    else
        hoverUv_ = { -1.0f, -1.0f };
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
    mouseMove (e);
}

void FringeAudioProcessorEditor::mouseUp (const juce::MouseEvent&)
{
    drawing_ = false;
}

void FringeAudioProcessorEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (fieldArea_.contains (e.getPosition()) && audioProcessor.getEngine().isDrawPreset())
    {
        audioProcessor.getEngine().clearDrawing();
        fieldDirty_ = true;
    }
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
        if (ch == m.key)
        {
            audioProcessor.getEngine().noteOn (m.note, 0.9f);
            return true;
        }

    if (key == juce::KeyPress::spaceKey)
    {
        if (auto* p = audioProcessor.getAPVTS().getParameter ("gate"))
            p->setValueNotifyingHost (p->getValue() < 0.5f ? 1.0f : 0.0f);
        return true;
    }
    return false;
}
