#include "FringeEngine.h"

namespace fringe
{

void FringeEngine::prepare (double sampleRate, int /*maxBlock*/)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    sim_.prepare (kDefaultW, kDefaultH);
    const int n = sim_.width() * sim_.height();
    speedScratch_.assign (static_cast<size_t> (n), 1.0f);
    drawLayer_.assign (static_cast<size_t> (n), 1.0f);
    colScratch_.assign (static_cast<size_t> (sim_.height()), 0.0f);
    hasDraw_ = false;
    for (auto& v : voices_)
        v.prepare (sampleRate_);
    fx_.prepare (sampleRate_);
    rebuildOptics();
    reset();
}

void FringeEngine::reset()
{
    sim_.reset();
    fx_.reset();
    for (auto& v : voices_)
        v.prepare (sampleRate_);
    envelope_ = 1.0f;
    sourceOn_ = true;
    sourceAmp_ = 0.02f;
    pulseSamplesLeft_ = 0;
    simAccum_ = 0.0;
}

EngineParams FringeEngine::getParams() const
{
    return params_;
}

bool FringeEngine::isDrawPreset() const
{
    return static_cast<Preset> (params_.preset) == Preset::Draw;
}

void FringeEngine::setParams (const EngineParams& p)
{
    const bool opticsDirty = p.preset != params_.preset
                             || std::abs (p.slit - params_.slit) > 1e-6f
                             || std::abs (p.slitW - params_.slitW) > 1e-6f;
    const bool leavingDraw = static_cast<Preset> (params_.preset) == Preset::Draw
                             && static_cast<Preset> (p.preset) != Preset::Draw;

    params_ = p;

    if (leavingDraw)
    {
        hasDraw_ = false;
        std::fill (drawLayer_.begin(), drawLayer_.end(), 1.0f);
    }

    // Don't rebuild over draw strokes while in Draw
    if (opticsDirty && static_cast<Preset> (params_.preset) != Preset::Draw)
        rebuildOptics();
    else if (opticsDirty && static_cast<Preset> (params_.preset) == Preset::Draw && ! hasDraw_)
        rebuildOptics();

    sim_.setSpeedMult (params_.speed);
    sim_.setSensitivity (params_.sensitivity);
}

void FringeEngine::rebuildOptics()
{
    std::lock_guard<std::mutex> lock (mapMutex_);
    const auto preset = static_cast<Preset> (std::clamp (params_.preset, 0, static_cast<int> (Preset::Count) - 1));
    OpticsBuilder::build (preset, params_.slit, params_.slitW, sim_.width(), sim_.height(), speedScratch_.data());

    if (hasDraw_ && preset == Preset::Draw)
    {
        // composite: wall if either base or draw is wall
        for (size_t i = 0; i < speedScratch_.size(); ++i)
            if (drawLayer_[i] < 0.5f)
                speedScratch_[i] = 0.0f;
    }

    sim_.setSpeedMap (speedScratch_.data(), sim_.width(), sim_.height());
}

void FringeEngine::paintAt (float uvX, float uvY, float brushUv, bool erase)
{
    if (! isDrawPreset())
        return;

    std::lock_guard<std::mutex> lock (mapMutex_);
    if (drawLayer_.size() != static_cast<size_t> (sim_.width() * sim_.height()))
        drawLayer_.assign (static_cast<size_t> (sim_.width() * sim_.height()), 1.0f);

    OpticsBuilder::paintDot (drawLayer_.data(), sim_.width(), sim_.height(), uvX, uvY, brushUv, erase);
    hasDraw_ = true;

    OpticsBuilder::build (Preset::Draw, params_.slit, params_.slitW, sim_.width(), sim_.height(), speedScratch_.data());
    for (size_t i = 0; i < speedScratch_.size(); ++i)
        if (drawLayer_[i] < 0.5f)
            speedScratch_[i] = 0.0f;

    sim_.setSpeedMap (speedScratch_.data(), sim_.width(), sim_.height());
}

void FringeEngine::clearDrawing()
{
    std::lock_guard<std::mutex> lock (mapMutex_);
    std::fill (drawLayer_.begin(), drawLayer_.end(), 1.0f);
    hasDraw_ = false;
    OpticsBuilder::build (static_cast<Preset> (params_.preset), params_.slit, params_.slitW,
                          sim_.width(), sim_.height(), speedScratch_.data());
    sim_.setSpeedMap (speedScratch_.data(), sim_.width(), sim_.height());
    sim_.reset();
}

void FringeEngine::noteOn (int note, float velocity)
{
    activeNote_ = note;
    params_.freq = midiNoteToSimFreq (note);
    sourceAmp_ = midiVelocityToAmp (std::clamp (velocity, 0.0f, 1.0f));
    sourceOn_ = true;
    envelope_ = 1.0f;

    if (params_.midiMode == 0)
        pulseSamplesLeft_ = static_cast<int> (0.010 * sampleRate_);
    else
        pulseSamplesLeft_ = -1;
}

void FringeEngine::noteOff (int note)
{
    if (params_.midiMode == 1 && (note == activeNote_ || note < 0))
    {
        sourceOn_ = false;
        pulseSamplesLeft_ = 0;
        activeNote_ = -1;
    }
}

void FringeEngine::applyGateAndEnvelope()
{
    if (pulseSamplesLeft_ > 0)
    {
        --pulseSamplesLeft_;
        if (pulseSamplesLeft_ == 0 && params_.midiMode == 0 && ! params_.gate)
            sourceOn_ = false;
    }

    const bool wantOn = params_.gate || pulseSamplesLeft_ > 0
                        || (params_.midiMode == 1 && activeNote_ >= 0);

    if (wantOn)
    {
        sourceOn_ = true;
        if (params_.gate && pulseSamplesLeft_ <= 0 && activeNote_ < 0)
            sourceAmp_ = 0.032f; // slightly fuller continuous body
        envelope_ += (1.0f - envelope_) * 0.1f;
    }
    else
    {
        sourceOn_ = false;
        const float rate = static_cast<float> (1.0 / sampleRate_ / std::max (0.05f, params_.release));
        envelope_ = std::max (0.0f, envelope_ - rate);
    }
}

void FringeEngine::tickLfos (int numSamples)
{
    const float dt = static_cast<float> (numSamples) / static_cast<float> (sampleRate_);
    for (auto& l : params_.lfos)
    {
        if (l.rate > 0.0001f)
        {
            l.phase += l.rate * dt;
            if (l.phase >= 1.0f)
                l.phase -= std::floor (l.phase);
        }
    }
}

EngineParams FringeEngine::modulatedParams() const
{
    EngineParams p = params_;
    auto apply = [] (float base, float minV, float maxV, float depth, float lfo) {
        if (depth <= 0.0f)
            return base;
        const float span = (maxV - minV) * depth * 0.5f;
        return std::clamp (base + lfo * span, minV, maxV);
    };

    for (const auto& l : params_.lfos)
    {
        if (l.depth <= 0.0f || l.rate <= 0.0f)
            continue;
        const float wave = std::sin (l.phase * 6.2831853f);
        switch (l.target)
        {
            case 0: p.freq = apply (p.freq, 15.0f, 100.0f, l.depth, wave); break;
            case 1: p.speed = apply (p.speed, 0.2f, 2.0f, l.depth, wave); break;
            case 2: p.slit = apply (p.slit, 0.008f, 0.15f, l.depth, wave); break;
            case 3: p.sensitivity = apply (p.sensitivity, 0.1f, 5.0f, l.depth, wave); break;
            case 4: p.filterHz = apply (p.filterHz, 200.0f, 12000.0f, l.depth, wave); break;
            case 5: p.reverb = apply (p.reverb, 0.0f, 1.0f, l.depth, wave); break;
            default: break;
        }
    }
    return p;
}

void FringeEngine::pushDetectors()
{
    const int h = sim_.height();
    if (static_cast<int> (colScratch_.size()) != h)
        colScratch_.assign (static_cast<size_t> (h), 0.0f);

    sim_.readDetectorColumn (kDetL, colScratch_.data(), h);
    voices_[0].pushColumn (colScratch_.data(), h);
    sim_.readDetectorColumn (kDetC, colScratch_.data(), h);
    voices_[1].pushColumn (colScratch_.data(), h);
    sim_.readDetectorColumn (kDetR, colScratch_.data(), h);
    voices_[2].pushColumn (colScratch_.data(), h);
}

void FringeEngine::process (float* left, float* right, int numSamples)
{
    if (left == nullptr || right == nullptr || numSamples <= 0)
        return;

    tickLfos (numSamples);
    const auto mod = modulatedParams();

    sim_.setSpeedMult (mod.speed);
    sim_.setSensitivity (mod.sensitivity);
    fx_.setFilterHz (mod.filterHz);
    fx_.setReverb (mod.reverb);
    fx_.setVolume (mod.volume);
    fx_.setScaleMode (mod.scaleMode);
    fx_.setDroneMode (mod.droneMode);

    // Rebuild optics if LFO is modulating slit on geometry presets
    static float lastSlit = -1.0f;
    if (std::abs (mod.slit - lastSlit) > 0.002f && static_cast<Preset> (params_.preset) != Preset::Draw)
    {
        lastSlit = mod.slit;
        const float saved = params_.slit;
        params_.slit = mod.slit;
        rebuildOptics();
        params_.slit = saved;
    }

    const double substepsWanted = static_cast<double> (numSamples) / sampleRate_ * 60.0 * kSubstepsPerBody;
    simAccum_ += substepsWanted;
    int subs = static_cast<int> (simAccum_);
    if (subs < 1)
        subs = 1;
    simAccum_ -= subs;
    if (subs > 28)
        subs = 28;

    applyGateAndEnvelope();
    const float amp = sourceAmp_ * envelope_;
    sim_.setSource (0.06f, mod.freq, amp, envelope_ > 0.001f, true);

    for (int s = 0; s < subs; ++s)
        sim_.substep();

    pushDetectors();

    for (int i = 0; i < numSamples; ++i)
    {
        applyGateAndEnvelope();
        const float l = voices_[0].processSample();
        const float c = voices_[1].processSample();
        const float r = voices_[2].processSample();
        fx_.process (l, c, r, voices_[1].energy(), left[i], right[i]);
    }

    snapCountdown_ -= numSamples;
    if (snapCountdown_ <= 0)
    {
        snapCountdown_ = static_cast<int> (sampleRate_ / 30.0);
        FieldSnapshot local;
        local.w = sim_.width();
        local.h = sim_.height();
        local.amp.resize (static_cast<size_t> (local.w * local.h));
        local.speed.resize (static_cast<size_t> (local.w * local.h));
        local.detector.resize (static_cast<size_t> (local.h));
        sim_.copyAmplitude (local.amp.data());
        sim_.copySpeed (local.speed.data());
        sim_.readDetectorColumn (kDetC, local.detector.data(), local.h);
        local.detectorX = sim_.detectorX();
        local.sourceX = sim_.sourceX();
        local.energy = voices_[1].energy();
        {
            std::lock_guard<std::mutex> lock (snapMutex_);
            snap_ = std::move (local);
            snapReady_.store (true, std::memory_order_release);
        }
    }
}

bool FringeEngine::pullSnapshot (FieldSnapshot& out)
{
    if (! snapReady_.load (std::memory_order_acquire))
        return false;
    std::lock_guard<std::mutex> lock (snapMutex_);
    out = snap_;
    snapReady_.store (false, std::memory_order_release);
    return out.w > 0 && out.h > 0;
}

} // namespace fringe
