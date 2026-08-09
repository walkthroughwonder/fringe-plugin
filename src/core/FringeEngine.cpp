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

    {   // [9b] preallocate so process() never allocates or frees
        std::lock_guard<std::mutex> lock (snapMutex_);
        snap_.w = sim_.width();
        snap_.h = sim_.height();
        snap_.amp.assign (static_cast<size_t> (n), 0.0f);
        snap_.speed.assign (static_cast<size_t> (n), 0.0f);
        snap_.detector.assign (static_cast<size_t> (sim_.height()), 0.0f);
        snapReady_.store (false, std::memory_order_release);
    }

    for (auto& v : voices_)
        v.prepare (sampleRate_);
    fx_.prepare (sampleRate_);

    opticsPreset_.store (params_.preset, std::memory_order_relaxed);
    opticsSlit_.store (params_.slit, std::memory_order_relaxed);
    opticsSlitW_.store (params_.slitW, std::memory_order_relaxed);
    uiSourceX_.store (params_.sourceX, std::memory_order_relaxed);
    uiDetectorX_.store (params_.detectorX, std::memory_order_relaxed);

    // prepare() is never concurrent with processBlock, so publishing straight
    // into sim_ here is safe; mapMutex_ still guards a concurrent editor paint.
    {
        std::lock_guard<std::mutex> lock (mapMutex_);
        buildOpticsLocked();
        sim_.setSpeedMap (speedScratch_.data(), sim_.width(), sim_.height());
        mapPending_.store (false, std::memory_order_release);
    }
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
    wavefrontPulse_ = false;
    simAccum_ = 0.0;
    lastSlit_ = -1.0f;   // force one optics resync after reset (slit range is 0.008..0.15)
}

EngineParams FringeEngine::getParams() const
{
    return params_;
}

bool FringeEngine::isDrawPreset() const
{
    // Called from the message thread; read the atomic mirror, not params_.
    return static_cast<Preset> (opticsPreset_.load (std::memory_order_acquire)) == Preset::Draw;
}

void FringeEngine::setParams (const EngineParams& p)
{
    const bool opticsDirty = p.preset != params_.preset
                             || std::abs (p.slit - params_.slit) > 1e-6f
                             || std::abs (p.slitW - params_.slitW) > 1e-6f;
    const bool leavingDraw = static_cast<Preset> (params_.preset) == Preset::Draw
                             && static_cast<Preset> (p.preset) != Preset::Draw;

    params_ = p;

    opticsPreset_.store (params_.preset, std::memory_order_release);
    opticsSlitW_.store (params_.slitW, std::memory_order_release);
    uiSourceX_.store (params_.sourceX, std::memory_order_release);
    uiDetectorX_.store (params_.detectorX, std::memory_order_release);

    // setParams runs on the AUDIO thread (processBlock -> pushParamsToEngine).
    // The 25k-element fill this used to do here raced paintAt on the message
    // thread; defer it to syncOptics, which holds mapMutex_.
    if (leavingDraw)
        clearDrawPending_.store (true, std::memory_order_release);

    // Don't rebuild over draw strokes while in Draw
    if (opticsDirty && (static_cast<Preset> (params_.preset) != Preset::Draw
                        || ! hasDraw_.load (std::memory_order_acquire)))
    {
        opticsSlit_.store (params_.slit, std::memory_order_release);
        lastSlit_ = params_.slit;
        requestOpticsRebuild();
    }

    sim_.setSpeedMult (params_.speed);
    sim_.setSensitivity (params_.sensitivity);
    sim_.setDetectorX (std::clamp (params_.detectorX, 0.55f, 0.98f));
}

// Message thread: update the UI mirror only. The real value reaches the engine
// via APVTS -> pushParamsToEngine() -> setParams() on the audio thread, which
// arrives on the very next block. Writing params_/sim_ from here raced the
// running callback.
void FringeEngine::setSourceX (float uvX)
{
    uiSourceX_.store (std::clamp (uvX, 0.02f, 0.45f), std::memory_order_release);
}

void FringeEngine::setDetectorX (float uvX)
{
    uiDetectorX_.store (std::clamp (uvX, 0.55f, 0.98f), std::memory_order_release);
}

float FringeEngine::getSourceX() const { return uiSourceX_.load (std::memory_order_acquire); }
float FringeEngine::getDetectorX() const { return uiDetectorX_.load (std::memory_order_acquire); }

void FringeEngine::requestOpticsRebuild()
{
    opticsDirty_.store (true, std::memory_order_release);
}

// Caller holds mapMutex_. Fills speedScratch_ only — never touches sim_, so it
// is safe to run from either thread. Publishing into sim_ is syncOptics's job.
void FringeEngine::buildOpticsLocked()
{
    const auto preset = static_cast<Preset> (std::clamp (opticsPreset_.load (std::memory_order_acquire),
                                                         0, static_cast<int> (Preset::Count) - 1));
    OpticsBuilder::build (preset,
                          opticsSlit_.load (std::memory_order_acquire),
                          opticsSlitW_.load (std::memory_order_acquire),
                          sim_.width(), sim_.height(), speedScratch_.data());

    if (hasDraw_.load (std::memory_order_acquire) && preset == Preset::Draw)
    {
        // composite: wall if either base or draw is wall
        for (size_t i = 0; i < speedScratch_.size(); ++i)
            if (drawLayer_[i] < 0.5f)
                speedScratch_[i] = 0.0f;
    }

    mapPending_.store (true, std::memory_order_release);
}

// Audio thread only. Never blocks: a failed try_lock leaves the flags set and
// the work is retried next block, costing at most one block of stale optics.
void FringeEngine::syncOptics()
{
    if (! opticsDirty_.load (std::memory_order_acquire)
        && ! mapPending_.load (std::memory_order_acquire)
        && ! simResetPending_.load (std::memory_order_acquire)
        && ! clearDrawPending_.load (std::memory_order_acquire))
        return;                       // fast path: no lock attempt at all

    std::unique_lock<std::mutex> lock (mapMutex_, std::try_to_lock);
    if (! lock.owns_lock())
        return;                       // UI is mid-paint; retry next block

    if (clearDrawPending_.exchange (false, std::memory_order_acq_rel))
    {
        std::fill (drawLayer_.begin(), drawLayer_.end(), 1.0f);
        hasDraw_.store (false, std::memory_order_release);
        opticsDirty_.store (true, std::memory_order_release);
    }

    if (opticsDirty_.exchange (false, std::memory_order_acq_rel))
        buildOpticsLocked();

    if (mapPending_.exchange (false, std::memory_order_acq_rel))
        sim_.setSpeedMap (speedScratch_.data(), sim_.width(), sim_.height());

    if (simResetPending_.exchange (false, std::memory_order_acq_rel))
        sim_.reset();
}

void FringeEngine::paintAt (float uvX, float uvY, float brushUv, bool erase)
{
    if (! isDrawPreset())
        return;

    std::lock_guard<std::mutex> lock (mapMutex_);
    if (drawLayer_.size() != static_cast<size_t> (sim_.width() * sim_.height()))
        drawLayer_.assign (static_cast<size_t> (sim_.width() * sim_.height()), 1.0f);

    OpticsBuilder::paintDot (drawLayer_.data(), sim_.width(), sim_.height(), uvX, uvY, brushUv, erase);
    hasDraw_.store (true, std::memory_order_release);
    buildOpticsLocked();
}

void FringeEngine::clearDrawing()
{
    std::lock_guard<std::mutex> lock (mapMutex_);
    std::fill (drawLayer_.begin(), drawLayer_.end(), 1.0f);
    hasDraw_.store (false, std::memory_order_release);
    buildOpticsLocked();
    // sim_.reset() memset the FDTD buffers under a running substep(); let the
    // audio thread do it.
    simResetPending_.store (true, std::memory_order_release);
}

void FringeEngine::noteOn (int note, float velocity)
{
    activeNote_ = note;
    params_.freq = midiNoteToSimFreq (note);
    sourceAmp_ = midiVelocityToAmp (std::clamp (velocity, 0.0f, 1.0f));
    sourceOn_ = true;
    envelope_ = 1.0f;
    wavefrontPulse_ = false;

    if (params_.midiMode == 0)
        pulseSamplesLeft_ = static_cast<int> (0.010 * sampleRate_);
    else
        pulseSamplesLeft_ = -1;
}

void FringeEngine::fireWavefront (float velocity)
{
    // Spacebar / manual strike: short, strong source packet → clear propagating front
    const float v = std::clamp (velocity, 0.0f, 1.0f);
    sourceAmp_ = 0.035f + v * 0.055f; // stronger than continuous drip
    sourceOn_ = true;
    envelope_ = 1.0f;
    wavefrontPulse_ = true;
    // ~45 ms of injection — enough cycles at low sim freq for a visible crest
    pulseSamplesLeft_ = static_cast<int> (0.045 * sampleRate_);
    // Don't latch enhanced hold note
    if (params_.midiMode != 1)
        activeNote_ = -1;
}

// --- UI -> audio thread handoff -------------------------------------------
// noteOn/fireWavefront mutate envelope_, sourceAmp_, pulseSamplesLeft_ and
// activeNote_, which applyGateAndEnvelope() reads per sample. Calling them
// from the message thread raced the callback, so the editor queues instead
// and the audio thread applies the event at the top of the next block.
// A single slot is sufficient: two events inside one block already overwrote
// each other before this change.

void FringeEngine::noteOnFromUi (int note, float velocity)
{
    uiNoteVel_.store (std::clamp (velocity, 0.0f, 1.0f), std::memory_order_relaxed);
    uiNote_.store (note, std::memory_order_release);
}

void FringeEngine::fireWavefrontFromUi (float velocity)
{
    uiWavefrontVel_.store (std::clamp (velocity, 0.0f, 1.0f), std::memory_order_relaxed);
    uiWavefront_.store (1, std::memory_order_release);
}

void FringeEngine::drainUiRequests()
{
    const int n = uiNote_.exchange (-1, std::memory_order_acq_rel);
    if (n >= 0)
        noteOn (n, uiNoteVel_.load (std::memory_order_relaxed));

    if (uiWavefront_.exchange (0, std::memory_order_acq_rel) != 0)
        fireWavefront (uiWavefrontVel_.load (std::memory_order_relaxed));
}

void FringeEngine::noteOff (int note)
{
    if (params_.midiMode == 1 && (note == activeNote_ || note < 0))
    {
        sourceOn_ = false;
        pulseSamplesLeft_ = 0;
        activeNote_ = -1;
        wavefrontPulse_ = false;
    }
}

void FringeEngine::applyGateAndEnvelope()
{
    if (pulseSamplesLeft_ > 0)
    {
        --pulseSamplesLeft_;
        if (pulseSamplesLeft_ == 0)
        {
            wavefrontPulse_ = false;
            // After a wavefront pulse: return to continuous gate level or off
            if (params_.midiMode == 0 && ! params_.gate && activeNote_ < 0)
                sourceOn_ = false;
        }
    }

    const bool wantOn = params_.gate || pulseSamplesLeft_ > 0
                        || (params_.midiMode == 1 && activeNote_ >= 0);

    if (wantOn)
    {
        sourceOn_ = true;
        // Don't stomp amplitude while a wavefront/MIDI pulse is active
        if (params_.gate && pulseSamplesLeft_ <= 0 && activeNote_ < 0 && ! wavefrontPulse_)
            sourceAmp_ = 0.032f;
        envelope_ += (1.0f - envelope_) * 0.15f;
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

    // L/C/R around movable center detector
    const float c = std::clamp (params_.detectorX, 0.55f, 0.98f);
    const float l = std::clamp (c - 0.04f, 0.50f, 0.97f);
    const float r = std::clamp (c + 0.04f, 0.53f, 0.99f);

    sim_.readDetectorColumn (l, colScratch_.data(), h);
    voices_[0].pushColumn (colScratch_.data(), h);
    sim_.readDetectorColumn (c, colScratch_.data(), h);
    voices_[1].pushColumn (colScratch_.data(), h);
    sim_.readDetectorColumn (r, colScratch_.data(), h);
    voices_[2].pushColumn (colScratch_.data(), h);
}

void FringeEngine::process (float* left, float* right, int numSamples)
{
    if (left == nullptr || right == nullptr || numSamples <= 0)
        return;

    drainUiRequests();          // [11] apply queued UI note/wavefront events here

    tickLfos (numSamples);
    const auto mod = modulatedParams();

    sim_.setSpeedMult (mod.speed);
    sim_.setSensitivity (mod.sensitivity);
    fx_.setFilterHz (mod.filterHz);
    fx_.setReverb (mod.reverb);
    fx_.setVolume (mod.volume);
    fx_.setScaleMode (mod.scaleMode);
    fx_.setDroneMode (mod.droneMode);

    // Rebuild optics if LFO is modulating slit on geometry presets.
    // lastSlit_ is per-instance now; the old function-local static was shared
    // by every FringeEngine in the process, so two plugin instances corrupted
    // each other's tracking and raced on it. The params_.slit save/restore
    // hack is gone too — it briefly published a value the message thread
    // could read mid-swap.
    if (std::abs (mod.slit - lastSlit_) > 0.002f
        && static_cast<Preset> (opticsPreset_.load (std::memory_order_relaxed)) != Preset::Draw)
    {
        lastSlit_ = mod.slit;
        opticsSlit_.store (mod.slit, std::memory_order_release);
        requestOpticsRebuild();
    }

    syncOptics();               // [9a] the only place that publishes geometry into sim_

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
    sim_.setDetectorX (std::clamp (params_.detectorX, 0.55f, 0.98f));
    sim_.setSource (std::clamp (params_.sourceX, 0.02f, 0.45f), mod.freq, amp, envelope_ > 0.001f, true);

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
        // [9b] No allocation, no free, no blocking. This used to resize three
        // vectors (~196 KB of malloc) and then move-assign into snap_, which
        // also FREED snap_'s previous buffers — all on the audio thread, ~30x
        // a second, inside a blocking lock. Buffers are preallocated in
        // prepare(); a failed try_lock simply drops one 30 Hz visual frame.
        const size_t n = static_cast<size_t> (sim_.width() * sim_.height());
        if (snap_.amp.size() == n && snap_.speed.size() == n
            && snap_.detector.size() == static_cast<size_t> (sim_.height()))
        {
            std::unique_lock<std::mutex> lock (snapMutex_, std::try_to_lock);
            if (lock.owns_lock())
            {
                snap_.w = sim_.width();
                snap_.h = sim_.height();
                sim_.copyAmplitude (snap_.amp.data());
                sim_.copySpeed (snap_.speed.data());
                sim_.readDetectorColumn (kDetC, snap_.detector.data(), snap_.h);
                snap_.detectorX = sim_.detectorX();
                snap_.sourceX = sim_.sourceX();
                snap_.energy = voices_[1].energy();
                snapReady_.store (true, std::memory_order_release);
            }
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
