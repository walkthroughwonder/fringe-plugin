#include "PluginEditor.h"

namespace
{
const juce::Colour kVoid   { 0xff06060a };
const juce::Colour kPanel  { 0xff0c0c12 };
const juce::Colour kRail   { 0xff0a0a10 };
const juce::Colour kBone   { 0xffe4d8b8 };
const juce::Colour kMuted  { 0x66e4d8b8 };
const juce::Colour kGold   { 0xffc9a84c };
const juce::Colour kGoldDim{ 0x55c9a84c };
const juce::Colour kCyan   { 0xff5ec8e8 };
const juce::Colour kLine   { 0x22ffffff };

constexpr double kAspect = 20.0 / 9.0;
constexpr float kHandleHitPx = 14.0f;
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
}

void FringeAudioProcessorEditor::setParamFloat (const juce::String& id, float value)
{
    if (auto* p = audioProcessor.getAPVTS().getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

FringeAudioProcessorEditor::FringeAudioProcessorEditor (FringeAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    constrainer_.setFixedAspectRatio (kAspect);
    constrainer_.setSizeLimits (1000, 450, 2400, 1080);
    setConstrainer (&constrainer_);
    setResizable (true, true);
    setSize (1280, 576);
    setWantsKeyboardFocus (true);

    brandLabel.setText ("FRINGE  1.0", juce::dontSendNotification);
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
    presetBox.addItemList ({ "Single Slit", "Double S lit", "Convex Lens", "Diffraction",
                             "Mach-Zehnder", "Draw", "Open Field" },
                           1);
    // fix typo
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
    styleToggle (viewButton);
    viewButton.setClickingTogglesState (true);
    viewButton.setButtonText ("CINE");
    viewButton.setTooltip ("Cinematic / Scientific field view");
    viewButton.onClick = [this] {
        scientificView_ = viewButton.getToggleState();
        viewButton.setButtonText (scientificView_ ? "SCI" : "CINE");
        fieldDirty_ = true;
        repaint();
    };

    clearDrawButton.setClickingTogglesState (false);
    clearDrawButton.setColour (juce::TextButton::buttonColourId, kPanel.brighter (0.04f));
    clearDrawButton.setColour (juce::TextButton::textColourOffId, kMuted);

    for (auto* b : { &gateButton, &scaleButton, &droneButton, &eraserButton, &clearDrawButton, &viewButton })
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

float FringeAudioProcessorEditor::xToUv (float x, juce::Rectangle<int> inner) const
{
    return std::clamp ((x - (float) inner.getX()) / (float) juce::jmax (1, inner.getWidth()), 0.0f, 1.0f);
}

float FringeAudioProcessorEditor::uvToX (float uv, juce::Rectangle<int> inner) const
{
    return (float) inner.getX() + uv * (float) inner.getWidth();
}

FringeAudioProcessorEditor::DragTarget FringeAudioProcessorEditor::hitTestProbes (juce::Point<float> pos,
                                                                                  juce::Rectangle<int> inner) const
{
    if (! inner.contains (pos.toInt()))
        return DragTarget::none;

    const float sx = uvToX (snapshot_.w > 0 ? snapshot_.sourceX : audioProcessor.getEngine().getSourceX(), inner);
    const float dx = uvToX (snapshot_.w > 0 ? snapshot_.detectorX : audioProcessor.getEngine().getDetectorX(), inner);

    if (std::abs (pos.x - sx) <= kHandleHitPx)
        return DragTarget::source;
    if (std::abs (pos.x - dx) <= kHandleHitPx)
        return DragTarget::detector;
    return DragTarget::none;
}

float FringeAudioProcessorEditor::sampleField (const std::vector<float>& data, int w, int h, float fx, float fy)
{
    // Bilinear sample; fx/fy in continuous pixel coords [0, w-1] x [0, h-1]
    if (w < 2 || h < 2 || data.empty())
        return 0.0f;
    fx = std::clamp (fx, 0.0f, (float) (w - 1));
    fy = std::clamp (fy, 0.0f, (float) (h - 1));
    const int x0 = (int) fx;
    const int y0 = (int) fy;
    const int x1 = std::min (x0 + 1, w - 1);
    const int y1 = std::min (y0 + 1, h - 1);
    const float tx = fx - (float) x0;
    const float ty = fy - (float) y0;
    const float a00 = data[(size_t) (y0 * w + x0)];
    const float a10 = data[(size_t) (y0 * w + x1)];
    const float a01 = data[(size_t) (y1 * w + x0)];
    const float a11 = data[(size_t) (y1 * w + x1)];
    const float a0 = a00 + (a10 - a00) * tx;
    const float a1 = a01 + (a11 - a01) * tx;
    return a0 + (a1 - a0) * ty;
}

float FringeAudioProcessorEditor::sampleSpeed (const std::vector<float>& data, int w, int h, float fx, float fy)
{
    // Nearest for walls (keep edges crisp), slight bilinear for media
    if (w < 1 || h < 1 || data.empty())
        return 1.0f;
    const int x = std::clamp ((int) std::lround (fx), 0, w - 1);
    const int y = std::clamp ((int) std::lround (fy), 0, h - 1);
    return data[(size_t) (y * w + x)];
}

juce::Colour FringeAudioProcessorEditor::scientificColour (float amp) const
{
    // Diverging blue–black–amber (classic wave lab look)
    const float a = std::clamp (amp, -1.0f, 1.0f);
    if (a >= 0.0f)
    {
        const float t = std::pow (a, 0.7f);
        return juce::Colour::fromFloatRGBA (0.05f + 0.95f * t,
                                            0.08f + 0.75f * t,
                                            0.12f + 0.25f * t,
                                            1.0f);
    }
    const float t = std::pow (-a, 0.7f);
    return juce::Colour::fromFloatRGBA (0.05f + 0.1f * t,
                                        0.12f + 0.45f * t,
                                        0.18f + 0.75f * t,
                                        1.0f);
}

juce::Colour FringeAudioProcessorEditor::amplitudeColour (float amp, float uvX, float energy) const
{
    if (scientificView_)
        return scientificColour (amp);

    // Signed amp in [-1,1] after normalize. Use intensity + polarity.
    const float mag = std::abs (amp);
    // Soft knee gain so quiet fringes still show, peaks don't blow out
    const float gain = 2.8f + energy * 1.5f;
    float t = std::clamp (mag * gain, 0.0f, 1.0f);
    t = t * (0.35f + 0.65f * t); // extra contrast curve

    const float age = std::clamp (uvX, 0.0f, 1.0f);

    // Cathedral age palette (closer to web WaveEngine)
    auto ramp = [] (float u, juce::Colour lo, juce::Colour mid, juce::Colour hi, juce::Colour pk) {
        if (u < 0.25f) return lo.interpolatedWith (mid, u / 0.25f);
        if (u < 0.6f)  return mid.interpolatedWith (hi, (u - 0.25f) / 0.35f);
        return hi.interpolatedWith (pk, (u - 0.6f) / 0.4f);
    };

    juce::Colour posYoung = ramp (t, juce::Colour (0xff0a1218), juce::Colour (0xff1f596b),
                                  juce::Colour (0xff4dc6d9), juce::Colour (0xffb8f0ff));
    juce::Colour posMid   = ramp (t, juce::Colour (0xff14100a), juce::Colour (0xff735828),
                                  juce::Colour (0xfff2c64d), juce::Colour (0xfffff0c8));
    juce::Colour posOld   = ramp (t, juce::Colour (0xff140a10), juce::Colour (0xff6b2840),
                                  juce::Colour (0xffe06090), juce::Colour (0xffffc0d8));

    juce::Colour negYoung = ramp (t, juce::Colour (0xff060a0c), juce::Colour (0xff0a1e28),
                                  juce::Colour (0xff1a4c66), juce::Colour (0xff3a8ca0));
    juce::Colour negMid   = ramp (t, juce::Colour (0xff0a0806), juce::Colour (0xff3a2810),
                                  juce::Colour (0xff8a6020), juce::Colour (0xffc8a050));
    juce::Colour negOld   = ramp (t, juce::Colour (0xff0c0608), juce::Colour (0xff3a1020),
                                  juce::Colour (0xff8a2848), juce::Colour (0xffc06080));

    juce::Colour pos = age < 0.4f ? posYoung.interpolatedWith (posMid, age / 0.4f)
                                  : posMid.interpolatedWith (posOld, (age - 0.4f) / 0.6f);
    juce::Colour neg = age < 0.4f ? negYoung.interpolatedWith (negMid, age / 0.4f)
                                  : negMid.interpolatedWith (negOld, (age - 0.4f) / 0.6f);

    juce::Colour c = amp >= 0.0f ? pos : neg;

    // faint zero-line darkness for fringe readability
    if (mag < 0.04f)
        c = juce::Colour (0xff06060c).interpolatedWith (c, mag / 0.04f);

    return c;
}

juce::Image FringeAudioProcessorEditor::renderFieldImage() const
{
    if (snapshot_.w <= 0 || snapshot_.amp.empty())
        return {};

    const int sw = snapshot_.w;
    const int sh = snapshot_.h;
    // Supersample 3x for smooth fringes without heavier FDTD
    constexpr int kSS = 3;
    const int dw = sw * kSS;
    const int dh = sh * kSS;
    juce::Image img (juce::Image::ARGB, dw, dh, true);
    const float energy = snapshot_.energy;

    // Robust normalize: use 95th percentile-ish via max with soft floor
    float maxA = 1.0e-5f;
    for (float a : snapshot_.amp)
        maxA = std::max (maxA, std::abs (a));
    // EMA-ish soft max so flashing peaks don't darken the whole field
    maxA = std::max (maxA, 1.0e-4f);
    const float invMax = 1.0f / maxA;

    const float detX = snapshot_.detectorX;
    const float srcX = snapshot_.sourceX;

    for (int y = 0; y < dh; ++y)
    {
        for (int x = 0; x < dw; ++x)
        {
            // continuous sim coords
            const float fx = ((float) x + 0.5f) / (float) kSS - 0.5f;
            const float fy = ((float) y + 0.5f) / (float) kSS - 0.5f;
            const float uvX = ((float) x + 0.5f) / (float) dw;
            const float uvY = ((float) y + 0.5f) / (float) dh;

            const float spd = sampleSpeed (snapshot_.speed, sw, sh, fx, fy);
            const float amp = sampleField (snapshot_.amp, sw, sh, fx, fy) * invMax;

            juce::Colour c;
            if (spd < 0.08f)
            {
                // Walls: matte body + soft edge via speed gradient
                const float sL = sampleSpeed (snapshot_.speed, sw, sh, fx - 0.6f, fy);
                const float sR = sampleSpeed (snapshot_.speed, sw, sh, fx + 0.6f, fy);
                const float sU = sampleSpeed (snapshot_.speed, sw, sh, fx, fy - 0.6f);
                const float sD = sampleSpeed (snapshot_.speed, sw, sh, fx, fy + 0.6f);
                const float edge = std::max ({ sL, sR, sU, sD });
                if (scientificView_)
                {
                    c = juce::Colour (0xff1a1814);
                    if (edge > 0.3f)
                        c = juce::Colour (0xffc8b890);
                }
                else
                {
                    c = juce::Colour (0xff0c0a12);
                    if (edge > 0.3f)
                        c = juce::Colour (0xffc9a84c).interpolatedWith (c, 0.25f);
                }
            }
            else if (spd < 0.75f)
            {
                // Media / beamsplitter tint
                c = amplitudeColour (amp, uvX, energy);
                const juce::Colour media = scientificView_ ? juce::Colour (0xff203040)
                                                           : juce::Colour (0xff142830);
                c = c.interpolatedWith (media, 0.22f * (1.0f - spd));
            }
            else
            {
                c = amplitudeColour (amp, uvX, energy);
            }

            // Source plane: soft cyan sheet
            {
                const float d = std::abs (uvX - srcX);
                if (d < 0.012f)
                {
                    const float g = 1.0f - d / 0.012f;
                    c = c.interpolatedWith (kCyan, g * g * 0.35f);
                }
            }

            // Detector plane: thin gold/yellow
            {
                const float d = std::abs (uvX - detX);
                if (d < 0.008f)
                {
                    const float g = 1.0f - d / 0.008f;
                    c = c.interpolatedWith (scientificView_ ? juce::Colours::yellow : kGold, g * g * 0.55f);
                }
            }

            // Subtle vertical gradient (depth cue) without crushing contrast
            if (! scientificView_)
            {
                const float vFade = 0.92f + 0.08f * std::sin (uvY * 3.14159f);
                c = c.withMultipliedBrightness (vFade);
            }

            img.setPixelAt (x, dh - 1 - y, c);
        }
    }

    // Peak-only micro glow (does not mush the whole field)
    if (! scientificView_)
    {
        juce::Image glow = img.createCopy();
        for (int y = 1; y < dh - 1; y += 2)
        {
            for (int x = 1; x < dw - 1; x += 2)
            {
                auto p = img.getPixelAt (x, y);
                const float lum = p.getFloatRed() * 0.25f + p.getFloatGreen() * 0.55f + p.getFloatBlue() * 0.2f;
                if (lum < 0.62f)
                    continue;
                const float a = (lum - 0.62f) * 0.4f;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        auto q = glow.getPixelAt (x + dx, y + dy);
                        glow.setPixelAt (x + dx, y + dy, q.interpolatedWith (p, a * 0.12f));
                    }
            }
        }
        return glow;
    }

    return img;
}

void FringeAudioProcessorEditor::paintChrome (juce::Graphics& g)
{
    g.fillAll (kVoid);
    juce::ColourGradient grad (kPanel, 0, 0, kVoid, 0, (float) getHeight() * 0.35f, false);
    g.setGradientFill (grad);
    g.fillRect (0, 0, getWidth(), getHeight() / 3);
    g.setColour (kGold.withAlpha (0.22f));
    g.drawRect (getLocalBounds().reduced (1), 1);
    g.setColour (kLine);
    g.drawRect (getLocalBounds().reduced (3), 1);
}

void FringeAudioProcessorEditor::paintProbeHandles (juce::Graphics& g, juce::Rectangle<int> inner)
{
    if (snapshot_.w <= 0 && fieldCache_.isNull())
        return;

    const float srcUv = snapshot_.w > 0 ? snapshot_.sourceX : audioProcessor.getEngine().getSourceX();
    const float detUv = snapshot_.w > 0 ? snapshot_.detectorX : audioProcessor.getEngine().getDetectorX();
    const float sx = uvToX (srcUv, inner);
    const float dx = uvToX (detUv, inner);

    auto drawHandle = [&] (float x, const juce::String& label, juce::Colour col, bool top) {
        g.setColour (col.withAlpha (0.55f + 0.2f * std::sin (animPhase_)));
        g.drawLine (x, (float) inner.getY() + 4, x, (float) inner.getBottom() - 4, 1.6f);

        const float cy = top ? (float) inner.getY() + 18.0f : (float) inner.getBottom() - 18.0f;
        g.setColour (col.withAlpha (0.9f));
        g.fillEllipse (x - 6.0f, cy - 6.0f, 12.0f, 12.0f);
        g.setColour (kVoid);
        g.drawEllipse (x - 6.0f, cy - 6.0f, 12.0f, 12.0f, 1.0f);

        g.setFont (juce::FontOptions (9.0f));
        g.setColour (col);
        g.drawText (label, juce::Rectangle<float> (x - 18, top ? cy + 8 : cy - 20, 36, 12),
                    juce::Justification::centred);
    };

    drawHandle (sx, "SRC", kCyan, true);
    drawHandle (dx, "DET", kGold, false);

    // L/R ghost detectors
    g.setColour (kGold.withAlpha (0.2f));
    g.drawLine (uvToX (std::clamp (detUv - 0.04f, 0.5f, 0.99f), inner), (float) inner.getY() + 8,
                uvToX (std::clamp (detUv - 0.04f, 0.5f, 0.99f), inner), (float) inner.getBottom() - 8, 1.0f);
    g.drawLine (uvToX (std::clamp (detUv + 0.04f, 0.5f, 0.99f), inner), (float) inner.getY() + 8,
                uvToX (std::clamp (detUv + 0.04f, 0.5f, 0.99f), inner), (float) inner.getBottom() - 8, 1.0f);
}

void FringeAudioProcessorEditor::paintWaveField (juce::Graphics& g, juce::Rectangle<int> area)
{
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
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (fieldCache_, inner.toFloat());

        if (! scientificView_)
        {
            juce::ColourGradient vig (juce::Colours::transparentBlack,
                                      (float) inner.getCentreX(), (float) inner.getCentreY(),
                                      juce::Colours::black.withAlpha (0.12f),
                                      (float) inner.getX(), (float) inner.getY(), true);
            g.setGradientFill (vig);
            g.fillRect (inner);
        }

        paintProbeHandles (g, inner);
    }

    if (audioProcessor.getEngine().isDrawPreset() && hoverUv_.x >= 0.0f && drag_ != DragTarget::source
        && drag_ != DragTarget::detector)
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

    g.setColour (kGold.withAlpha (0.28f));
    g.drawRoundedRectangle (area.toFloat(), 4.0f, 1.0f);

    g.setColour (kMuted);
    g.setFont (juce::FontOptions (9.0f));
    juce::String hint = scientificView_ ? "SCI view  |  " : "CINE view  |  ";
    hint += "drag SRC / DET handles";
    if (audioProcessor.getEngine().isDrawPreset())
        hint += eraser_ ? "  |  DRAW eraser" : "  |  DRAW paint (dbl-click clear)";
    g.drawText (hint, area.reduced (10).removeFromTop (16), juce::Justification::topLeft);
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

    auto meter = r.removeFromTop (8);
    g.setColour (kVoid);
    g.fillRoundedRectangle (meter.toFloat(), 2.0f);
    const float e = std::clamp (snapshot_.energy * 8.0f, 0.0f, 1.0f);
    g.setColour (kGold.interpolatedWith (kCyan, e));
    g.fillRoundedRectangle (meter.toFloat().withWidth (meter.getWidth() * e), 2.0f);

    r.removeFromTop (6);
    g.setColour (kMuted);
    g.drawText ("energy  " + juce::String (snapshot_.energy, 3), r.removeFromTop (12), juce::Justification::centredLeft);
    g.drawText ("src " + juce::String (audioProcessor.getEngine().getSourceX(), 2)
                    + "  det " + juce::String (audioProcessor.getEngine().getDetectorX(), 2),
                r.removeFromTop (12), juce::Justification::centredLeft);

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

    struct KnobInfo { juce::Slider* s; const char* depthId; };
    // LFO depth glow
    auto lfoDepth = [this] (const juce::String& id) -> float {
        if (auto* v = audioProcessor.getAPVTS().getRawParameterValue (id))
            return v->load();
        return 0.0f;
    };

    for (auto* s : { &volumeSlider, &speedSlider, &freqSlider, &slitSlider, &slitWSlider,
                     &sensSlider, &filterSlider, &reverbSlider,
                     &lfo1Rate, &lfo1Depth, &lfo2Rate, &lfo2Depth, &lfo3Rate, &lfo3Depth })
    {
        auto b = s->getBounds();
        g.setColour (kMuted);
        g.drawText (s->getName(), b.getX(), b.getBottom() - 1, b.getWidth(), 11, juce::Justification::centred);
    }

    // glow rings for active LFOs
    auto maybeGlow = [&] (juce::Slider& s, float depth) {
        if (depth <= 0.01f)
            return;
        auto b = s.getBounds().toFloat().reduced (8.0f, 14.0f);
        const float cx = b.getCentreX();
        const float cy = b.getCentreY() - 4.0f;
        const float rad = juce::jmin (b.getWidth(), b.getHeight()) * 0.42f;
        g.setColour (kCyan.withAlpha (0.15f + 0.25f * depth));
        g.drawEllipse (cx - rad - 3, cy - rad - 3, (rad + 3) * 2, (rad + 3) * 2, 1.5f);
    };
    maybeGlow (lfo1Rate, lfoDepth ("lfo1Depth"));
    maybeGlow (lfo1Depth, lfoDepth ("lfo1Depth"));
    maybeGlow (lfo2Rate, lfoDepth ("lfo2Depth"));
    maybeGlow (lfo2Depth, lfoDepth ("lfo2Depth"));
    maybeGlow (lfo3Rate, lfoDepth ("lfo3Depth"));
    maybeGlow (lfo3Depth, lfoDepth ("lfo3Depth"));

    if (! knobRail_.isEmpty())
    {
        g.setColour (kGold.withAlpha (0.55f));
        g.setFont (juce::FontOptions (9.0f));
        const int cell = knobRail_.getWidth() / 14;
        const int y = knobRail_.getY() - 13;
        g.drawText ("TONE", knobRail_.getX(), y, cell * 4, 12, juce::Justification::centred);
        g.drawText ("FIELD", knobRail_.getX() + cell * 4, y, cell * 4, 12, juce::Justification::centred);
        g.drawText ("MOD", knobRail_.getX() + cell * 8, y, cell * 6, 12, juce::Justification::centred);
    }
}

void FringeAudioProcessorEditor::paint (juce::Graphics& g)
{
    paintChrome (g);

    if (! fieldArea_.isEmpty())
        paintWaveField (g, fieldArea_);
    if (! detectorArea_.isEmpty())
        paintDetectorPanel (g, detectorArea_);

    if (! knobRail_.isEmpty())
    {
        g.setColour (kRail);
        g.fillRoundedRectangle (knobRail_.toFloat().expanded (6.0f, 4.0f), 8.0f);
        g.setColour (kLine);
        g.drawRoundedRectangle (knobRail_.toFloat().expanded (6.0f, 4.0f), 8.0f, 1.0f);
    }

    paintKnobRailLabels (g);

    if (! statusBar_.isEmpty())
    {
        g.setColour (kMuted);
        g.setFont (juce::FontOptions (10.0f));
        juce::String status = "20:9  |  ";
        status += scientificView_ ? "SCI  |  " : "CINE  |  ";
        status += gateButton.getToggleState() ? "source on  |  " : "source off  |  ";
        status += "drag SRC/DET  |  keys Z-P  |  space = wavefront";
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
        tagLabel.setBounds (t.removeFromLeft (150));
        clearDrawButton.setBounds (t.removeFromRight (56).reduced (2, 4));
        eraserButton.setBounds (t.removeFromRight (56).reduced (2, 4));
        viewButton.setBounds (t.removeFromRight (52).reduced (2, 4));
        droneButton.setBounds (t.removeFromRight (60).reduced (2, 4));
        scaleButton.setBounds (t.removeFromRight (60).reduced (2, 4));
        gateButton.setBounds (t.removeFromRight (68).reduced (2, 4));
        presetBox.setBounds (t.removeFromRight (140).reduced (2, 4));
    }

    statusBar_ = r.removeFromBottom (18);
    r.removeFromBottom (2);
    r.removeFromBottom (14);
    const int railH = juce::jlimit (100, 128, r.getHeight() * 26 / 100);
    knobRail_ = r.removeFromBottom (railH);
    r.removeFromBottom (8);

    const int detW = juce::jlimit (120, 170, r.getWidth() * 14 / 100);
    detectorArea_ = r.removeFromRight (detW);
    r.removeFromRight (10);
    fieldArea_ = r;

    juce::Slider* knobs[] = {
        &volumeSlider, &speedSlider, &freqSlider, &filterSlider,
        &slitSlider, &slitWSlider, &sensSlider, &reverbSlider,
        &lfo1Rate, &lfo1Depth, &lfo2Rate, &lfo2Depth, &lfo3Rate, &lfo3Depth
    };
    auto rail = knobRail_.reduced (4, 6);
    const int cell = rail.getWidth() / 14;
    for (int i = 0; i < 14; ++i)
        knobs[i]->setBounds (rail.getX() + i * cell, rail.getY(), cell, rail.getHeight() - 10);
}

void FringeAudioProcessorEditor::paintAtEvent (const juce::MouseEvent& e)
{
    if (! fieldArea_.contains (e.getPosition()))
        return;
    if (! audioProcessor.getEngine().isDrawPreset())
        return;

    const auto inner = fieldArea_.reduced (2);
    const float uvX = xToUv ((float) e.x, inner);
    const float uvY = 1.0f - std::clamp (((float) e.y - (float) inner.getY()) / (float) juce::jmax (1, inner.getHeight()), 0.0f, 1.0f);
    float brush = 0.012f;
    if (auto* v = audioProcessor.getAPVTS().getRawParameterValue ("slitW"))
        brush = std::clamp (v->load(), 0.004f, 0.06f);

    audioProcessor.getEngine().paintAt (uvX, uvY, brush, eraser_);
    fieldDirty_ = true;
}

void FringeAudioProcessorEditor::mouseMove (const juce::MouseEvent& e)
{
    const auto inner = fieldArea_.reduced (2);
    if (inner.contains (e.getPosition()))
    {
        hoverUv_ = { xToUv ((float) e.x, inner),
                     1.0f - std::clamp (((float) e.y - (float) inner.getY()) / (float) juce::jmax (1, inner.getHeight()), 0.0f, 1.0f) };
        const auto hit = hitTestProbes (e.position, inner);
        setMouseCursor (hit != DragTarget::none ? juce::MouseCursor::LeftRightResizeCursor
                                                : juce::MouseCursor::NormalCursor);
    }
    else
    {
        hoverUv_ = { -1.0f, -1.0f };
        setMouseCursor (juce::MouseCursor::NormalCursor);
    }
}

void FringeAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto inner = fieldArea_.reduced (2);
    drag_ = hitTestProbes (e.position, inner);
    if (drag_ == DragTarget::none && audioProcessor.getEngine().isDrawPreset() && inner.contains (e.getPosition()))
    {
        drag_ = DragTarget::paint;
        paintAtEvent (e);
    }
}

void FringeAudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    const auto inner = fieldArea_.reduced (2);
    if (drag_ == DragTarget::source)
    {
        const float uv = std::clamp (xToUv ((float) e.x, inner), 0.02f, 0.45f);
        audioProcessor.getEngine().setSourceX (uv);
        setParamFloat ("sourceX", uv);
        snapshot_.sourceX = uv;
        fieldDirty_ = true;
    }
    else if (drag_ == DragTarget::detector)
    {
        const float uv = std::clamp (xToUv ((float) e.x, inner), 0.55f, 0.98f);
        audioProcessor.getEngine().setDetectorX (uv);
        setParamFloat ("detectorX", uv);
        snapshot_.detectorX = uv;
        fieldDirty_ = true;
    }
    else if (drag_ == DragTarget::paint)
    {
        paintAtEvent (e);
    }
    mouseMove (e);
}

void FringeAudioProcessorEditor::mouseUp (const juce::MouseEvent&)
{
    drag_ = DragTarget::none;
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
        // Emit a wavefront packet (does not toggle continuous SOURCE)
        audioProcessor.getEngine().fireWavefront (0.95f);
        return true;
    }

    return false;
}
