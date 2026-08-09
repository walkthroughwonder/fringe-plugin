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

    /** Audio thread only (MIDI path). */
    void noteOn (int note, float velocity);
    void noteOff (int note);
    /** Audio thread only. Fires a short source pulse — a visible/audible wavefront packet. */
    void fireWavefront (float velocity = 0.9f);
    /** Message thread: queue the event; the audio thread applies it next block. */
    void noteOnFromUi (int note, float velocity);
    void fireWavefrontFromUi (float velocity = 0.9f);
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
    // The optics rebuild is split so the audio thread never blocks and never
    // touches sim_ while the message thread might be building into scratch.
    void requestOpticsRebuild();   // any thread: sets a flag, does no work
    void buildOpticsLocked();      // any thread, caller holds mapMutex_; never touches sim_
    void syncOptics();             // audio thread only: try_lock, publish map/reset
    void drainUiRequests();        // audio thread only
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
    // Written by paintAt/clearDrawing (message thread), read by setParams and
    // buildOpticsLocked (audio thread).
    std::atomic<bool> hasDraw_ { false };

    bool sourceOn_ = true;
    float sourceAmp_ = 0.02f;
    float envelope_ = 1.0f;
    int pulseSamplesLeft_ = 0;
    int activeNote_ = -1;
    bool wavefrontPulse_ = false;

    double simAccum_ = 0.0;

    // [10] was `static float lastSlit` inside process(), i.e. shared by every
    // FringeEngine in the process. Now per-instance.
    float lastSlit_ = -1.0f;

    // Optics build inputs: published by the audio thread, read by whichever
    // thread runs buildOpticsLocked().
    std::atomic<int>   opticsPreset_ { 0 };
    std::atomic<float> opticsSlit_   { 0.035f };
    std::atomic<float> opticsSlitW_  { 0.014f };

    // Deferred work flags; the consumer is always the audio thread (syncOptics).
    std::atomic<bool> opticsDirty_      { false };
    std::atomic<bool> mapPending_       { false };
    std::atomic<bool> simResetPending_  { false };
    std::atomic<bool> clearDrawPending_ { false };

    // UI note events: message thread produces, audio thread consumes.
    std::atomic<int>   uiNote_         { -1 };
    std::atomic<float> uiNoteVel_      { 0.9f };
    std::atomic<int>   uiWavefront_    { 0 };
    std::atomic<float> uiWavefrontVel_ { 0.95f };

    // UI-readable mirrors of params_: audio thread writes, message thread reads.
    std::atomic<float> uiSourceX_   { 0.06f };
    std::atomic<float> uiDetectorX_ { kDetC };

    mutable std::mutex mapMutex_;
    mutable std::mutex snapMutex_;
    FieldSnapshot snap_;
    std::atomic<bool> snapReady_ { false };
    int snapCountdown_ = 0;
};

} // namespace fringe
