#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace fringe
{

// Balanced default; max for future HQ
inline constexpr int kMaxW = 256;
inline constexpr int kMaxH = 128;
inline constexpr int kDefaultW = 224;
inline constexpr int kDefaultH = 112;

inline constexpr double kSimDt = 1.0 / 60.0;
inline constexpr int kSubstepsPerBody = 12;
inline constexpr float kDamp = 0.9995f;
inline constexpr float kC2Scale = 0.24f;
inline constexpr float kC2Max = 0.49f;

inline constexpr float kDetL = 0.88f;
inline constexpr float kDetC = 0.92f;
inline constexpr float kDetR = 0.96f;

inline constexpr std::array<float, 4> kFdnTimes { 1.37f, 1.71f, 2.23f, 3.19f };
inline constexpr float kFdnFeedback = 0.55f;
inline constexpr float kFdnDarkenHz = 1600.0f; // darker cathedral tail

enum class Preset : int
{
    SingleSlit = 0,
    DoubleSlit,
    Lens,
    Diffraction,
    MachZehnder,
    Draw,
    Empty,
    Count
};

inline const char* presetName (Preset p)
{
    switch (p)
    {
        case Preset::SingleSlit:  return "Single Slit";
        case Preset::DoubleSlit:  return "Double Slit";
        case Preset::Lens:        return "Convex Lens";
        case Preset::Diffraction: return "Diffraction";
        case Preset::MachZehnder: return "Mach-Zehnder";
        case Preset::Draw:        return "Draw";
        case Preset::Empty:       return "Open Field";
        case Preset::Count:       return "?";
    }
    return "?";
}

struct LfoState
{
    float rate = 0.0f;   // Hz 0–8
    float depth = 0.0f;  // 0–1
    int target = 0;      // 0=freq 1=speed 2=slit 3=sens 4=filter 5=reverb
    float phase = 0.0f;
};

struct EngineParams
{
    // Warm / mid-bass factory character (musical, not screechy)
    float volume = 0.48f;
    float speed = 0.72f;
    float freq = 28.0f;           // slower source → denser, lower energy field
    float sensitivity = 1.15f;
    float filterHz = 1100.0f;     // low-pass ceiling in the musical mid/bass band
    float reverb = 0.58f;
    float release = 0.7f;
    float slit = 0.035f;
    float slitW = 0.014f;
    bool scaleMode = true;        // pentatonic resonance by default
    bool droneMode = false;
    bool gate = true;
    int preset = 0;               // Single Slit
    int midiMode = 0;
    std::array<LfoState, 3> lfos {};
};

struct FieldSnapshot
{
    int w = 0, h = 0;
    std::vector<float> amp;
    std::vector<float> speed;
    std::vector<float> detector; // center column for graph
    float detectorX = kDetC;
    float sourceX = 0.06f;
    float energy = 0.0f;
};

inline float midiNoteToHz (int note)
{
    return 440.0f * std::pow (2.0f, (static_cast<float> (note) - 69.0f) / 12.0f);
}

inline float midiNoteToSimFreq (int note)
{
    return std::clamp (15.0f + (static_cast<float> (note) - 21.0f) * (85.0f / 87.0f), 15.0f, 100.0f);
}

inline float midiVelocityToAmp (float vel01)
{
    return 0.002f + vel01 * vel01 * vel01 * 0.048f;
}

inline float quantizePentatonic (float freq)
{
    // Snap into C minor-pent-ish degrees, then fold into bass/mid (C2–C4-ish)
    static constexpr float kDegrees[] = {
        65.41f, 73.42f, 82.41f, 98.00f, 110.00f,   // C2 D2 E2 G2 A2
        130.81f, 146.83f, 164.81f, 196.00f, 220.00f, // C3…
        261.63f, 293.66f, 329.63f, 392.00f, 440.00f  // C4… (cap)
    };
    float best = kDegrees[0];
    float bestD = 1e9f;
    for (float d : kDegrees)
    {
        const float err = std::abs (std::log2 (std::max (freq, 1.0f) / d));
        if (err < bestD) { bestD = err; best = d; }
    }
    return best;
}

} // namespace fringe
