#include "FxChain.h"

namespace fringe
{

void FxChain::Biquad::setLowpass (float sr, float hz, float q)
{
    const float w0 = 2.0f * 3.14159265f * hz / sr;
    const float alpha = std::sin (w0) / (2.0f * q);
    const float cosw = std::cos (w0);
    const float a0 = 1.0f + alpha;
    b0 = ((1.0f - cosw) * 0.5f) / a0;
    b1 = (1.0f - cosw) / a0;
    b2 = b0;
    a1 = (-2.0f * cosw) / a0;
    a2 = (1.0f - alpha) / a0;
}

void FxChain::Biquad::setHighpass (float sr, float hz, float q)
{
    const float w0 = 2.0f * 3.14159265f * hz / sr;
    const float alpha = std::sin (w0) / (2.0f * q);
    const float cosw = std::cos (w0);
    const float a0 = 1.0f + alpha;
    b0 = ((1.0f + cosw) * 0.5f) / a0;
    b1 = (-(1.0f + cosw)) / a0;
    b2 = b0;
    a1 = (-2.0f * cosw) / a0;
    a2 = (1.0f - alpha) / a0;
}

void FxChain::Biquad::setPeaking (float sr, float hz, float q, float gainDb)
{
    const float A = std::pow (10.0f, gainDb / 40.0f);
    const float w0 = 2.0f * 3.14159265f * hz / sr;
    const float alpha = std::sin (w0) / (2.0f * q);
    const float cosw = std::cos (w0);
    const float a0 = 1.0f + alpha / A;
    b0 = (1.0f + alpha * A) / a0;
    b1 = (-2.0f * cosw) / a0;
    b2 = (1.0f - alpha * A) / a0;
    a1 = (-2.0f * cosw) / a0;
    a2 = (1.0f - alpha / A) / a0;
}

void FxChain::DelayLine::prepare (int maxSamples)
{
    buf.assign (static_cast<size_t> (std::max (1, maxSamples)), 0.0f);
    w = 0;
}

float FxChain::DelayLine::process (float x, float feedback, Biquad& darken)
{
    if (buf.empty())
        return x;
    float y = buf[static_cast<size_t> (w)];
    const float fb = darken.process (y) * feedback;
    buf[static_cast<size_t> (w)] = x + fb;
    w = (w + 1) % static_cast<int> (buf.size());
    return y;
}

void FxChain::prepare (double sampleRate)
{
    sr_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    for (int i = 0; i < 4; ++i)
    {
        const int n = static_cast<int> (kFdnTimes[static_cast<size_t> (i)] * sr_) + 1;
        fdn_[static_cast<size_t> (i)].prepare (n);
        fdnDarken_[static_cast<size_t> (i)].setLowpass (static_cast<float> (sr_), kFdnDarkenHz, 0.5f);
    }
    lpf_.setLowpass (static_cast<float> (sr_), filterHz_, 0.5f);
    hpf_.setHighpass (static_cast<float> (sr_), 20.0f, 0.5f);
    peakF_.setPeaking (static_cast<float> (sr_), 300.0f, 1.2f, 3.0f);
    peakFifth_.setPeaking (static_cast<float> (sr_), 450.0f, 2.0f, 0.0f);
    for (auto& d : dronePeaks_)
        d.setPeaking (static_cast<float> (sr_), 600.0f, 4.0f, 0.0f);
    reset();
}

void FxChain::reset()
{
    for (auto& d : fdn_)
        std::fill (d.buf.begin(), d.buf.end(), 0.0f);
    compEnv_ = 0.0f;
    subPhase_ = 0.0f;
}

void FxChain::process (float l, float c, float r, float energy, float& outL, float& outR)
{
    // Stereo mix of 3 voices (web pans -0.7, 0, 0.7 with gains 0.7, 1, 0.7)
    float mono = l * 0.7f * 0.5f + c * 1.0f + r * 0.7f * 0.5f;
    float left = l * 0.7f + c * 0.7f + r * 0.15f;
    float right = r * 0.7f + c * 0.7f + l * 0.15f;
    (void) mono;

    // Soft compressor
    const float level = std::abs (left) + std::abs (right);
    const float atk = 0.01f, rel = 0.15f;
    const float coeff = level > compEnv_ ? atk : rel;
    // one-pole approx using time constants roughly at sample rate
    const float a = 1.0f - std::exp (-1.0f / (static_cast<float> (sr_) * (level > compEnv_ ? 0.01f : 0.15f)));
    compEnv_ += (level - compEnv_) * a;
    float gain = 1.0f;
    if (compEnv_ > 0.1f) // ~ -20 dB threshold-ish linear
        gain = 0.1f / (compEnv_ + 1e-6f);
    gain = std::clamp (0.25f + 0.75f * std::min (1.0f, gain), 0.15f, 1.0f);
    left *= gain;
    right *= gain;

    // Energy → resonant freq (web: 60 + energy*2000, clamp 40–3500)
    float targetFreq = 60.0f + energy * 2000.0f;
    targetFreq = std::clamp (targetFreq, 40.0f, 3500.0f);
    if (scaleMode_)
        targetFreq = quantizePentatonic (targetFreq);
    smoothTargetFreq_ += (targetFreq - smoothTargetFreq_) * 0.08f;

    peakF_.setPeaking (static_cast<float> (sr_), smoothTargetFreq_, scaleMode_ ? 3.0f : 1.2f,
                       scaleMode_ ? 8.0f : 3.0f);
    float mid = 0.5f * (left + right);
    float peaked = peakF_.process (mid);

    if (scaleMode_ && ! droneMode_)
    {
        peakFifth_.setPeaking (static_cast<float> (sr_), std::min (3500.0f, smoothTargetFreq_ * 1.5f), 2.0f, 4.0f);
        peaked += peakFifth_.process (mid) * 0.5f;
    }

    if (droneMode_)
    {
        for (int i = 0; i < 3; ++i)
        {
            const float h = static_cast<float> (i + 2);
            const float g = std::min (8.0f, energy * (40.0f + 20.0f * static_cast<float> (i)));
            dronePeaks_[static_cast<size_t> (i)].setPeaking (
                static_cast<float> (sr_), std::min (3500.0f, smoothTargetFreq_ * h), 4.0f, g);
            peaked += dronePeaks_[static_cast<size_t> (i)].process (mid) * 0.35f;
        }
    }

    // FDN wet
    float wet = 0.0f;
    const float split = peaked * 0.25f;
    for (int i = 0; i < 4; ++i)
        wet += fdn_[static_cast<size_t> (i)].process (split, kFdnFeedback, fdnDarken_[static_cast<size_t> (i)]);
    wet *= 0.25f;

    const float dryGain = 1.0f - reverb_ * 0.6f;
    const float wetGain = reverb_ * 0.7f;
    float mixed = peaked * dryGain + wet * wetGain;

    // LPF then HPF (actual web graph)
    lpf_.setLowpass (static_cast<float> (sr_), filterHz_, 0.5f);
    mixed = lpf_.process (mixed);
    mixed = hpf_.process (mixed);

    // Sub osc bypasses LPF path → add after HPF on dry mix path conceptually after
    const float subFreq = std::max (20.0f, smoothTargetFreq_ * 0.5f);
    subPhase_ += subFreq / static_cast<float> (sr_);
    if (subPhase_ >= 1.0f)
        subPhase_ -= 1.0f;
    const float subLevel = std::min (0.15f, energy * 1.5f);
    const float sub = std::sin (subPhase_ * 6.2831853f) * subLevel;

    const float out = (mixed + sub) * volume_;
    // Light stereo from original L/R balance
    const float bal = 0.5f * (left - right);
    outL = out + bal * 0.15f;
    outR = out - bal * 0.15f;
}

} // namespace fringe
