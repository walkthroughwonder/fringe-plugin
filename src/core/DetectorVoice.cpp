#include "DetectorVoice.h"

namespace fringe
{

void DetectorVoice::prepare (double sampleRate)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    bufA_.clear();
    bufB_.clear();
    activeA_ = true;
    crossfade_ = 1.0f;
    readPos_ = 0.0;
    prevSample_ = 0.0f;
    smoothEnergy_ = 0.0f;
    localEnergy_ = 0.0f;
}

void DetectorVoice::pushColumn (const float* data, int len)
{
    if (data == nullptr || len <= 0)
        return;

    auto& dest = activeA_ ? bufB_ : bufA_;
    dest.assign (data, data + len);
    activeA_ = ! activeA_;
    crossfade_ = 0.0f;
}

float DetectorVoice::processSample()
{
    const auto& data = activeA_ ? bufA_ : bufB_;
    const auto& fade = activeA_ ? bufB_ : bufA_;

    if (data.empty())
        return 0.0f;

    const int len = static_cast<int> (data.size());

    // Energy
    float totalEnergy = 0.0f;
    for (float v : data)
        totalEnergy += v * v;
    totalEnergy = std::sqrt (totalEnergy / static_cast<float> (len));
    localEnergy_ += (totalEnergy - localEnergy_) * 0.03f;
    smoothEnergy_ = localEnergy_;
    const float envGain = std::min (1.0f, localEnergy_ * 15.0f);

    const int posFloor = static_cast<int> (std::floor (readPos_));
    const float frac = static_cast<float> (readPos_ - static_cast<double> (posFloor));
    const int idx0 = ((posFloor % len) + len) % len;
    const int idx1 = ((posFloor + 1) % len + len) % len;

    float sample = data[static_cast<size_t> (idx0)]
                   + (data[static_cast<size_t> (idx1)] - data[static_cast<size_t> (idx0)]) * frac;

    if (crossfade_ < 1.0f && ! fade.empty())
    {
        const int fLen = static_cast<int> (fade.size());
        const int f0 = ((posFloor % fLen) + fLen) % fLen;
        const int f1 = ((posFloor + 1) % fLen + fLen) % fLen;
        const float fVal = fade[static_cast<size_t> (f0)]
                           + (fade[static_cast<size_t> (f1)] - fade[static_cast<size_t> (f0)]) * frac;
        sample = sample * crossfade_ + fVal * (1.0f - crossfade_);
    }

    // ScriptProcessor path: tanh * 1.8
    sample = std::tanh (sample * 1.8f) * envGain;
    sample = prevSample_ * 0.1f + sample * 0.9f;
    prevSample_ = sample;

    readPos_ += 1.0; // playbackRate 1.0
    if (readPos_ >= static_cast<double> (len) * 1000.0)
        readPos_ -= static_cast<double> (len) * 1000.0;

    if (crossfade_ < 1.0f)
        crossfade_ = std::min (1.0f, crossfade_ + static_cast<float> (1.0 / sampleRate_ * 8.0));

    return sample * 0.35f;
}

} // namespace fringe
