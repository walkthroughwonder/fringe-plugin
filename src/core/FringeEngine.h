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
    EngineParams getParams() const;

    void noteOn (int note, float velocity);
    void noteOff (int note);
    void process (float* left, float* right, int numSamples);

    bool pullSnapshot (FieldSnapshot& out);

    /** Draw mode: paint walls into the live speed map (message/UI thread OK via mutex). */
    void paintAt (float uvX, float uvY, float brushUv, bool erase);
    void clearDrawing();
    bool isDrawPreset() const;

    /** Drag probes on the field (message thread). Clamped UV. */
    void setSourceX (float uvX);
    void setDetectorX (float uvX);
    float getSourceX() const;
    float getDetectorX() const;

    int gridW() const { return sim_.width(); }
    int gridH() const { return sim_.height(); }
    double sampleRate() const { return sampleRate_; }

private:
    void rebuildOptics();
    void applyGateAndEnvelope();
    void pushDetectors();
    void tickLfos (int numSamples);
    EngineParams modulatedParams() const;

    double sampleRate_ = 44100.0;
    EngineParams params_;
    FdtdSimulator sim_;
    std::array<DetectorVoice, 3> voices_;
    FxChain fx_;

    std::vector<float> speedScratch_;
    std::vector<float> drawLayer_; // persistent draw strokes
    std::vector<float> colScratch_;
    bool hasDraw_ = false;

    bool sourceOn_ = true;
    float sourceAmp_ = 0.02f;
    float envelope_ = 1.0f;
    int pulseSamplesLeft_ = 0;
    int activeNote_ = -1;

    double simAccum_ = 0.0;

    mutable std::mutex mapMutex_;
    mutable std::mutex snapMutex_;
    FieldSnapshot snap_;
    std::atomic<bool> snapReady_ { false };
    int snapCountdown_ = 0;
};

} // namespace fringe
