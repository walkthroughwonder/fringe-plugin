#pragma once

#include "FringeTypes.h"
#include <vector>

namespace fringe
{

/** ScriptProcessor-style column sonification from audio-engine.js */
class DetectorVoice
{
public:
    void prepare (double sampleRate);
    void pushColumn (const float* data, int len);
    float processSample();

    float energy() const { return smoothEnergy_; }

private:
    std::vector<float> bufA_, bufB_;
    bool activeA_ = true;
    float crossfade_ = 1.0f;
    double readPos_ = 0.0;
    float prevSample_ = 0.0f;
    float smoothEnergy_ = 0.0f;
    float localEnergy_ = 0.0f;
    double sampleRate_ = 44100.0;
};

} // namespace fringe
