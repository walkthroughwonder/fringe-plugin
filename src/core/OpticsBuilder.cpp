#include "OpticsBuilder.h"

namespace fringe
{

void OpticsBuilder::fill (float* m, int n, float v)
{
    for (int i = 0; i < n; ++i)
        m[i] = v;
}

void OpticsBuilder::wallRect (float* m, int w, int h, float x0, float y0, float x1, float y1)
{
    const int ix0 = std::clamp (static_cast<int> (x0 * w), 0, w - 1);
    const int ix1 = std::clamp (static_cast<int> (x1 * w), 0, w);
    const int iy0 = std::clamp (static_cast<int> (y0 * h), 0, h - 1);
    const int iy1 = std::clamp (static_cast<int> (y1 * h), 0, h);

    for (int y = iy0; y < iy1; ++y)
        for (int x = ix0; x < ix1; ++x)
            m[y * w + x] = 0.0f;
}

void OpticsBuilder::lensBlob (float* m, int w, int h, float cx, float cy, float radius, float ior)
{
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            const float u = (static_cast<float> (x) + 0.5f) / static_cast<float> (w);
            const float v = (static_cast<float> (y) + 0.5f) / static_cast<float> (h);
            const float aspect = static_cast<float> (w) / static_cast<float> (h);
            const float dx = (u - cx) * aspect;
            const float dy = v - cy;
            const float r = std::sqrt (dx * dx + dy * dy);
            if (r < radius)
            {
                // Lower speed inside lens (ior-ish)
                const float t = 1.0f - r / radius;
                m[y * w + x] = std::clamp (1.0f - t * (1.0f - ior), 0.15f, 1.0f);
            }
        }
    }
}

void OpticsBuilder::build (Preset preset, float slit, float slitW, int w, int h, float* speedOut)
{
    const int n = w * h;
    fill (speedOut, n, 1.0f);

    const float wallX = 0.15f;
    const float wallThick = 0.006f;

    switch (preset)
    {
        case Preset::SingleSlit:
        {
            const float slitWidth = std::clamp (slit, 0.008f, 0.08f);
            wallRect (speedOut, w, h, wallX, 0.0f, wallX + wallThick, 0.5f - slitWidth * 0.5f);
            wallRect (speedOut, w, h, wallX, 0.5f + slitWidth * 0.5f, wallX + wallThick, 1.0f);
            break;
        }
        case Preset::DoubleSlit:
        {
            const float sep = std::clamp (slit, 0.02f, 0.15f);
            const float sw = std::clamp (slitW, 0.004f, 0.06f);
            // three wall segments
            wallRect (speedOut, w, h, wallX, 0.0f, wallX + wallThick, 0.5f - sep * 0.5f - sw * 0.5f);
            wallRect (speedOut, w, h, wallX, 0.5f - sep * 0.5f + sw * 0.5f, wallX + wallThick,
                      0.5f + sep * 0.5f - sw * 0.5f);
            wallRect (speedOut, w, h, wallX, 0.5f + sep * 0.5f + sw * 0.5f, wallX + wallThick, 1.0f);
            break;
        }
        case Preset::Lens:
        {
            const float ior = std::clamp (slit, 0.3f, 0.65f);
            lensBlob (speedOut, w, h, 0.20f, 0.5f, 0.12f, ior);
            break;
        }
        case Preset::Diffraction:
        {
            const float aperture = std::clamp (slit, 0.02f, 0.15f);
            wallRect (speedOut, w, h, wallX, 0.0f, wallX + wallThick, 0.42f + aperture);
            break;
        }
        case Preset::Empty:
        default:
            break;
    }
}

} // namespace fringe
