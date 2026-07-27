#include "FringeEngine.h"

namespace fringe
{

void FringeEngine::prepare (double sampleRate, int /*maxBlock*/)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    sim_.prepare (kDefaultW, kDefaultH);
    speedScratch_.assign (static_cast<size_t> (kDefaultW * kDefaultH), 1.0f);
    colScratch_.assign (static_cast<size_t> (kDefaultH), 0.0f);
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

void FringeEngine::setParams (const EngineParams& p)
{
    const bool opticsDirty = p.preset != params_.preset
                             || std::abs (p.slit - params_.slit) > 1e-6f
                             || std::abs (p.slitW - params_.slitW) > 1e-6f;
    params_ = p;
    if (opticsDirty)
        rebuildOptics();

    sim_.setSpeedMult (params_.speed);
    sim_.setSensitivity (params_.sensitivity);
    fx_.setFilterHz (params_.filterHz);
    fx_.setReverb (params_.reverb);
    fx_.setVolume (params_.volume);
    fx_.setScaleMode (params_.scaleMode);
    fx_.setDroneMode (params_.droneMode);
}

void FringeEngine::rebuildOptics()
{
    const auto preset = static_cast<Preset> (std::clamp (params_.preset, 0, static_cast<int> (Preset::Count) - 1));
    OpticsBuilder::build (preset, params_.slit, params_.slitW, sim_.width(), sim_.height(), speedScratch_.data());
    sim_.setSpeedMap (speedScratch_.data(), sim_.width(), sim_.height());
}

void FringeEngine::noteOn (int note, float velocity)
{
    activeNote_ = note;
    params_.freq = midiNoteToSimFreq (note);
    sourceAmp_ = midiVelocityToAmp (std::clamp (velocity, 0.0f, 1.0f));
    sourceOn_ = true;
    envelope_ = 1.0f;

    if (params_.midiMode == 0)
    {
        // Web-parity: 10 ms pulse
        pulseSamplesLeft_ = static_cast<int> (0.010 * sampleRate_);
    }
    else
    {
        pulseSamplesLeft_ = -1; // held until noteOff
    }
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

    // Continuous gate (web SOURCE on) OR active pulse/held note
    const bool wantOn = params_.gate || pulseSamplesLeft_ > 0
                        || (params_.midiMode == 1 && activeNote_ >= 0);

    if (wantOn)
    {
        sourceOn_ = true;
        if (params_.gate && pulseSamplesLeft_ <= 0 && activeNote_ < 0)
            sourceAmp_ = 0.02f; // default continuous amp
        envelope_ += (1.0f - envelope_) * 0.1f;
    }
    else
    {
        sourceOn_ = false;
        const float rate = static_cast<float> (1.0 / sampleRate_ / std::max (0.05f, params_.release));
        envelope_ = std::max (0.0f, envelope_ - rate);
    }
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

    // How many FDTD substeps for this block (~720/s like web)
    const double substepsWanted = static_cast<double> (numSamples) / sampleRate_ * 60.0 * kSubstepsPerBody;
    simAccum_ += substepsWanted;
    int subs = static_cast<int> (simAccum_);
    if (subs < 1)
        subs = 1;
    simAccum_ -= subs;
    if (subs > 24)
        subs = 24; // safety cap

    applyGateAndEnvelope();
    const float amp = sourceAmp_ * envelope_;
    sim_.setSource (0.06f, params_.freq, amp, envelope_ > 0.001f && (sourceOn_ || envelope_ > 0.01f), true);

    for (int s = 0; s < subs; ++s)
        sim_.substep();

    pushDetectors();

    for (int i = 0; i < numSamples; ++i)
    {
        applyGateAndEnvelope();
        const float l = voices_[0].processSample();
        const float c = voices_[1].processSample();
        const float r = voices_[2].processSample();
        const float energy = voices_[1].energy();
        fx_.process (l, c, r, energy, left[i], right[i]);
    }

    // Snapshot for editor ~30 Hz
    snapCountdown_ -= numSamples;
    if (snapCountdown_ <= 0)
    {
        snapCountdown_ = static_cast<int> (sampleRate_ / 30.0);
        FieldSnapshot local;
        local.w = sim_.width();
        local.h = sim_.height();
        local.amp.resize (static_cast<size_t> (local.w * local.h));
        local.speed.resize (static_cast<size_t> (local.w * local.h));
        sim_.copyAmplitude (local.amp.data());
        sim_.copySpeed (local.speed.data());
        local.detectorX = sim_.detectorX();
        local.sourceX = sim_.sourceX();
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
