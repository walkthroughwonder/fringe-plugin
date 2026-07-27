#pragma once

#include "FringeTypes.h"
#include <cstring>

namespace fringe
{

/** CPU FDTD port of wave-engine.js wave update shader. */
class FdtdSimulator
{
public:
    void prepare (int width, int height);
    void reset();
    void setSpeedMap (const float* map, int w, int h); // values 0..1

    void setSource (float uvX, float freq, float amp, bool continuous, bool planeWave);
    void setSpeedMult (float m) { speedMult_ = m; }
    void setDetectorX (float x) { detectorX_ = x; }
    void setSensitivity (float s) { sensitivity_ = s; }

    /** Run one full body (kSubstepsPerBody inner steps) or N raw substeps. */
    void stepBody();
    void substep();

    void readDetectorColumn (float uvX, float* outInstant, int outH) const;
    void copyAmplitude (float* dest) const;
    void copySpeed (float* dest) const;

    int width() const { return w_; }
    int height() const { return h_; }
    double simTime() const { return simTime_; }
    float sourceX() const { return sourceX_; }
    float detectorX() const { return detectorX_; }

private:
    int idx (int x, int y) const { return y * w_ + x; }
    float sample (const std::vector<float>& f, int x, int y) const;

    int w_ = 0, h_ = 0;
    std::vector<float> curr_, prev_, out_, speed_;
    double simTime_ = 0.0;
    float speedMult_ = 0.9f;
    float sourceX_ = 0.06f;
    float sourceFreq_ = 60.0f;
    float sourceAmp_ = 0.02f;
    float detectorX_ = kDetC;
    float sensitivity_ = 1.0f;
    bool continuous_ = true;
    bool planeWave_ = true;
};

} // namespace fringe
