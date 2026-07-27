#pragma once

#include "DetectorVoice.h"
#include "FdtdSimulator.h"
#include "FxChain.h"
#include "OpticsBuilder.h"
#include <atomic>
#include <mutex>

namespace fringe
{

class FringeEngine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();

    void setParams (const EngineParams& p);
    EngineParams getParams() const { return params_; }

    void noteOn (int note, float velocity);
    void noteOff (int note);
    void process (float* left, float* right, int numSamples);

    /** Editor-safe copy of field for painting (rate-limited by caller). */
    bool pullSnapshot (FieldSnapshot& out);

    double sampleRate() const { return sampleRate_; }

private:
    void rebuildOptics();
    void applyGateAndEnvelope();
    void pushDetectors();

    double sampleRate_ = 44100.0;
    EngineParams params_;
    FdtdSimulator sim_;
    std::array<DetectorVoice, 3> voices_;
    FxChain fx_;

    std::vector<float> speedScratch_;
    std::vector<float> colScratch_;

    // MIDI / gate
    bool sourceOn_ = true;
    float sourceAmp_ = 0.02f;
    float envelope_ = 1.0f;
    int pulseSamplesLeft_ = 0;
    int activeNote_ = -1;

    // Sim scheduling
    double simAccum_ = 0.0;

    // Snapshot for UI
    mutable std::mutex snapMutex_;
    FieldSnapshot snap_;
    std::atomic<bool> snapReady_ { false };
    int snapCountdown_ = 0;
};

} // namespace fringe
