#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace fringe
{

// Grid tiers (ultraplan). Default Balanced-ish for laptop CPU.
inline constexpr int kMaxW = 256;
inline constexpr int kMaxH = 128;
inline constexpr int kDefaultW = 192;
inline constexpr int kDefaultH = 96;

inline constexpr double kSimDt = 1.0 / 60.0;
inline constexpr int kSubstepsPerBody = 12;
inline constexpr float kDamp = 0.9995f;
inline constexpr float kC2Scale = 0.24f;
inline constexpr float kC2Max = 0.49f;

// Detector columns (web main.js)
inline constexpr float kDetL = 0.88f;
inline constexpr float kDetC = 0.92f;
inline constexpr float kDetR = 0.96f;

// FDN (audio-engine.js)
inline constexpr std::array<float, 4> kFdnTimes { 1.37f, 1.71f, 2.23f, 3.19f };
inline constexpr float kFdnFeedback = 0.55f;
inline constexpr float kFdnDarkenHz = 2800.0f;

enum class Preset : int
{
    SingleSlit = 0,
    DoubleSlit,
    Lens,
    Diffraction,
    Empty, // free field / draw later
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
        case Preset::Empty:       return "Open Field";
        default:                  return "?";
    }
}

struct EngineParams
{
    float volume = 0.3f;
    float speed = 0.9f;
    float freq = 60.0f;          // sim source freq 15–100
    float sensitivity = 1.0f;
    float filterHz = 7000.0f;
    float reverb = 0.4f;
    float release = 0.5f;
    float slit = 0.03f;          // gap / width / curvature depending on preset
    float slitW = 0.012f;
    bool scaleMode = false;
    bool droneMode = false;
    bool gate = true;   // continuous source (web continuousSource)
    int preset = 0;
    int midiMode = 0; // 0=parity pulse, 1=enhanced hold
};

struct FieldSnapshot
{
    int w = 0, h = 0;
    std::vector<float> amp;   // w*h amplitude
    std::vector<float> speed; // w*h speed map
    float detectorX = kDetC;
    float sourceX = 0.06f;
};

inline float midiNoteToHz (int note)
{
    return 440.0f * std::pow (2.0f, (static_cast<float> (note) - 69.0f) / 12.0f);
}

// Web main.js mapping
inline float midiNoteToSimFreq (int note)
{
    return std::clamp (15.0f + (static_cast<float> (note) - 21.0f) * (85.0f / 87.0f), 15.0f, 100.0f);
}

inline float midiVelocityToAmp (float vel01)
{
    // 0.002 + vel^3 * 0.048
    return 0.002f + vel01 * vel01 * vel01 * 0.048f;
}

inline float quantizePentatonic (float freq)
{
    static constexpr int semis[] = { 0, 2, 4, 7, 9 };
    const float A4 = 440.0f;
    const float semiFromA4 = 12.0f * std::log2 (std::max (freq, 1.0f) / A4);
    const float noteInOctave = std::fmod (semiFromA4 + 1200.0f, 12.0f);
    float best = 0, bestD = 1e9f;
    for (int s : semis)
    {
        const float d = std::abs (noteInOctave - static_cast<float> (s));
        if (d < bestD) { bestD = d; best = static_cast<float> (s); }
    }
    const float octave = std::floor ((semiFromA4 + 3.0f) / 12.0f);
    const float n = octave * 12.0f + best - 3.0f; // invert semiFromA4 offset-ish
    return A4 * std::pow (2.0f, n / 12.0f);
}

} // namespace fringe
