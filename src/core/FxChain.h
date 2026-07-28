#pragma once

#include "FringeTypes.h"
#include <array>
#include <vector>

namespace fringe
{

/** Compressor → peaking EQ bank → FDN reverb → LPF → HPF + sub-osc */
class FxChain
{
public:
    void prepare (double sampleRate);
    void reset();

    void setFilterHz (float hz) { filterHz_ = hz; }
    void setReverb (float dryWet) { reverb_ = dryWet; }
    void setVolume (float v) { volume_ = v; }
    void setScaleMode (bool on) { scaleMode_ = on; }
    void setDroneMode (bool on) { droneMode_ = on; }

    /** Process stereo from 3 mono voices (L, C, R). */
    void process (float l, float c, float r, float energy,
                  float& outL, float& outR);

private:
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float z1 = 0, z2 = 0;
        float process (float x)
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void setLowpass (float sr, float hz, float q);
        void setHighpass (float sr, float hz, float q);
        void setPeaking (float sr, float hz, float q, float gainDb);
    };

    struct DelayLine
    {
        std::vector<float> buf;
        int w = 0;
        void prepare (int maxSamples);
        float process (float x, float feedback, Biquad& darken);
    };

    double sr_ = 44100.0;
    float filterHz_ = 1100.0f;
    float reverb_ = 0.58f;
    float volume_ = 0.48f;
    bool scaleMode_ = false;
    bool droneMode_ = false;

    Biquad lpf_, hpf_;
    Biquad peakF_, peakFifth_;
    std::array<Biquad, 3> dronePeaks_;
    std::array<DelayLine, 4> fdn_;
    std::array<Biquad, 4> fdnDarken_;

    float subPhase_ = 0.0f;
    float smoothTargetFreq_ = 300.0f;

    // Simple compressor state
    float compEnv_ = 0.0f;
};

} // namespace fringe
