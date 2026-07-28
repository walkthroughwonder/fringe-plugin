#include "FdtdSimulator.h"

namespace fringe
{

void FdtdSimulator::prepare (int width, int height)
{
    w_ = std::clamp (width, 32, kMaxW);
    h_ = std::clamp (height, 32, kMaxH);
    const int n = w_ * h_;
    curr_.assign (n, 0.0f);
    prev_.assign (n, 0.0f);
    out_.assign (n, 0.0f);
    speed_.assign (n, 1.0f);
    simTime_ = 0.0;
}

void FdtdSimulator::reset()
{
    std::fill (curr_.begin(), curr_.end(), 0.0f);
    std::fill (prev_.begin(), prev_.end(), 0.0f);
    std::fill (out_.begin(), out_.end(), 0.0f);
    simTime_ = 0.0;
}

void FdtdSimulator::setSpeedMap (const float* map, int w, int h)
{
    if (w != w_ || h != h_ || map == nullptr)
        return;
    std::memcpy (speed_.data(), map, static_cast<size_t> (w_ * h_) * sizeof (float));
}

void FdtdSimulator::setSource (float uvX, float freq, float amp, bool continuous, bool planeWave)
{
    sourceX_ = uvX;
    sourceFreq_ = freq;
    sourceAmp_ = amp;
    continuous_ = continuous;
    planeWave_ = planeWave;
}

float FdtdSimulator::sample (const std::vector<float>& f, int x, int y) const
{
    if (x < 0 || y < 0 || x >= w_ || y >= h_)
        return 0.0f;
    return f[static_cast<size_t> (idx (x, y))];
}

void FdtdSimulator::substep()
{
    if (w_ <= 2 || h_ <= 2)
        return;

    simTime_ += kSimDt / static_cast<double> (kSubstepsPerBody);
    const float t = static_cast<float> (simTime_);
    const float signal = continuous_ ? std::sin (sourceFreq_ * t) : 0.0f;
    const int sx = std::clamp (static_cast<int> (sourceX_ * static_cast<float> (w_)), 0, w_ - 1);
    const int detX = std::clamp (static_cast<int> (detectorX_ * static_cast<float> (w_)), 0, w_ - 1);
    const float detGain = speedMult_ * sensitivity_ * 20.0f;

    for (int y = 0; y < h_; ++y)
    {
        for (int x = 0; x < w_; ++x)
        {
            const int i = idx (x, y);
            const float spd = speed_[static_cast<size_t> (i)] * speedMult_;

            if (spd < 0.01f)
            {
                out_[static_cast<size_t> (i)] = 0.0f;
                continue;
            }

            const float c = curr_[static_cast<size_t> (i)];
            const float p = prev_[static_cast<size_t> (i)];
            const float up = sample (curr_, x, y + 1);
            const float down = sample (curr_, x, y - 1);
            const float right = sample (curr_, x + 1, y);
            const float left = sample (curr_, x - 1, y);

            float c2 = spd * kC2Scale;
            if (c2 > kC2Max)
                c2 = kC2Max;

            float next = 2.0f * c - p + c2 * (up + down + left + right - 4.0f * c);
            next *= kDamp;

            // Sponge top/bottom
            const float uvY = (static_cast<float> (y) + 0.5f) / static_cast<float> (h_);
            const float bmin = std::min (uvY, 1.0f - uvY);
            constexpr float borderWidth = 0.08f;
            if (bmin < borderWidth)
            {
                const float tt = bmin / borderWidth;
                next *= 0.15f + 0.85f * (tt * tt);
            }
            if (bmin < 0.02f)
                next *= bmin / 0.02f;

            const float uvX = (static_cast<float> (x) + 0.5f) / static_cast<float> (w_);
            if (uvX > detectorX_ + 0.01f)
            {
                const float rFade = 1.0f - std::clamp ((uvX - detectorX_) / (1.0f - detectorX_), 0.0f, 1.0f);
                next *= rFade;
            }

            // Soft source
            if (continuous_)
            {
                float env = 0.0f;
                if (planeWave_)
                {
                    const float dx = std::abs (uvX - sourceX_);
                    env = 1.0f - std::clamp (dx / 0.005f, 0.0f, 1.0f);
                }
                else
                {
                    const float aspect = static_cast<float> (w_) / static_cast<float> (h_);
                    const float dx = (uvX - sourceX_) * aspect;
                    const float dy = uvY - 0.5f;
                    const float r = std::sqrt (dx * dx + dy * dy);
                    env = 1.0f - std::clamp (r / 0.015f, 0.0f, 1.0f);
                }
                next += signal * env * sourceAmp_;
            }

            // Hard kill past detector
            if (x > detX)
                next = 0.0f;

            out_[static_cast<size_t> (i)] = next;
        }
    }

    // Rotate: prev = curr, curr = out
    prev_.swap (curr_);
    curr_.swap (out_);
    (void) sx;
    (void) detGain;
}

void FdtdSimulator::stepBody()
{
    for (int s = 0; s < kSubstepsPerBody; ++s)
        substep();
}

void FdtdSimulator::readDetectorColumn (float uvX, float* outInstant, int outH) const
{
    if (outInstant == nullptr || outH <= 0 || h_ <= 0)
        return;

    const int x = std::clamp (static_cast<int> (uvX * static_cast<float> (w_)), 0, w_ - 1);
    for (int oy = 0; oy < outH; ++oy)
    {
        const int y = std::clamp (static_cast<int> (static_cast<float> (oy) / static_cast<float> (outH) * static_cast<float> (h_)),
                                  0, h_ - 1);
        const float a = curr_[static_cast<size_t> (idx (x, y))];
        // Instant intensity-like signal (signed amp for richer spectrum)
        outInstant[oy] = a * sensitivity_ * speedMult_ * 3.2f; // was *8 — less brittle highs
    }
}

void FdtdSimulator::copyAmplitude (float* dest) const
{
    if (dest == nullptr)
        return;
    std::memcpy (dest, curr_.data(), static_cast<size_t> (w_ * h_) * sizeof (float));
}

void FdtdSimulator::copySpeed (float* dest) const
{
    if (dest == nullptr)
        return;
    std::memcpy (dest, speed_.data(), static_cast<size_t> (w_ * h_) * sizeof (float));
}

} // namespace fringe
