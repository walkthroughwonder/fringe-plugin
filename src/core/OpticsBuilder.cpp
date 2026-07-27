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
    const int ix0 = std::clamp (static_cast<int> (x0 * w), 0, w);
    const int ix1 = std::clamp (static_cast<int> (x1 * w), 0, w);
    const int iy0 = std::clamp (static_cast<int> (y0 * h), 0, h);
    const int iy1 = std::clamp (static_cast<int> (y1 * h), 0, h);

    for (int y = std::min (iy0, iy1); y < std::max (iy0, iy1); ++y)
        for (int x = std::min (ix0, ix1); x < std::max (ix0, ix1); ++x)
            if (x >= 0 && x < w && y >= 0 && y < h)
                m[y * w + x] = 0.0f;
}

void OpticsBuilder::wallSegment (float* m, int w, int h, float x0, float y0, float x1, float y1, float thickness)
{
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float len = std::sqrt (dx * dx + dy * dy);
    if (len < 1e-6f)
        return;
    const int steps = std::max (2, static_cast<int> (len * static_cast<float> (std::max (w, h)) * 2.0f));
    for (int i = 0; i <= steps; ++i)
    {
        const float t = static_cast<float> (i) / static_cast<float> (steps);
        paintDot (m, w, h, x0 + dx * t, y0 + dy * t, thickness * 0.5f, false);
    }
}

void OpticsBuilder::paintDot (float* speed, int w, int h, float uvX, float uvY, float radiusUv, bool erase)
{
    const int cx = static_cast<int> (uvX * w);
    const int cy = static_cast<int> (uvY * h);
    const int r = std::max (1, static_cast<int> (radiusUv * static_cast<float> (std::min (w, h))));
    const float val = erase ? 1.0f : 0.0f;

    for (int dy = -r; dy <= r; ++dy)
        for (int dx = -r; dx <= r; ++dx)
            if (dx * dx + dy * dy <= r * r)
            {
                const int x = cx + dx;
                const int y = cy + dy;
                if (x >= 0 && x < w && y >= 0 && y < h)
                    speed[y * w + x] = val;
            }
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
                const float t = 1.0f - r / radius;
                m[y * w + x] = std::clamp (1.0f - t * (1.0f - ior), 0.15f, 1.0f);
            }
        }
    }
}

void OpticsBuilder::beamsplitter (float* m, int w, int h, float cx, float cy, float angle,
                                  float length, float thickness, float ior)
{
    const float ca = std::cos (angle);
    const float sa = std::sin (angle);
    const float hx = ca * length * 0.5f;
    const float hy = sa * length * 0.5f;
    // Partial reflector: set speed to ior along segment
    const int steps = std::max (4, static_cast<int> (length * static_cast<float> (std::max (w, h)) * 3.0f));
    const int r = std::max (1, static_cast<int> (thickness * static_cast<float> (std::min (w, h))));

    for (int i = 0; i <= steps; ++i)
    {
        const float t = static_cast<float> (i) / static_cast<float> (steps);
        const float u = cx + (-hx + 2.0f * hx * t);
        const float v = cy + (-hy + 2.0f * hy * t);
        const int cxp = static_cast<int> (u * w);
        const int cyp = static_cast<int> (v * h);
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx)
                if (dx * dx + dy * dy <= r * r)
                {
                    const int x = cxp + dx;
                    const int y = cyp + dy;
                    if (x >= 0 && x < w && y >= 0 && y < h)
                        m[y * w + x] = ior;
                }
    }
}

void OpticsBuilder::build (Preset preset, float slit, float slitW, int w, int h, float* speedOut)
{
    const int n = w * h;
    fill (speedOut, n, 1.0f);

    const float wallX = 0.15f;
    const float wallThick = 0.008f;

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
            wallRect (speedOut, w, h, wallX, 0.0f, wallX + wallThick, 0.5f - sep * 0.5f - sw * 0.5f);
            wallRect (speedOut, w, h, wallX, 0.5f - sep * 0.5f + sw * 0.5f, wallX + wallThick,
                      0.5f + sep * 0.5f - sw * 0.5f);
            wallRect (speedOut, w, h, wallX, 0.5f + sep * 0.5f + sw * 0.5f, wallX + wallThick, 1.0f);
            break;
        }
        case Preset::Lens:
        {
            const float ior = std::clamp (slit, 0.3f, 0.65f);
            lensBlob (speedOut, w, h, 0.22f, 0.5f, 0.13f, ior);
            break;
        }
        case Preset::Diffraction:
        {
            const float aperture = std::clamp (slit, 0.02f, 0.15f);
            wallRect (speedOut, w, h, wallX, 0.0f, wallX + wallThick, 0.42f + aperture * 0.5f);
            break;
        }
        case Preset::MachZehnder:
        {
            // Static MZ layout (Path Δ deferred) — mirrors + beamsplitters from web main.js
            beamsplitter (speedOut, w, h, 0.18f, 0.5f, 0.785f, 0.10f, 0.008f, 0.55f);
            wallSegment (speedOut, w, h, 0.18f - 0.04f, 0.35f - 0.04f, 0.18f + 0.04f, 0.35f + 0.04f, 0.01f);
            wallSegment (speedOut, w, h, 0.45f - 0.04f, 0.5f - 0.04f, 0.45f + 0.04f, 0.5f + 0.04f, 0.01f);
            beamsplitter (speedOut, w, h, 0.45f, 0.35f, 0.785f, 0.10f, 0.008f, 0.55f);
            wallRect (speedOut, w, h, 0.13f, 0.0f, 0.13f + wallThick, 0.28f);
            wallRect (speedOut, w, h, 0.13f, 0.72f, 0.13f + wallThick, 1.0f);
            break;
        }
        case Preset::Draw:
        case Preset::Empty:
        case Preset::Count:
        default:
            break;
    }
}

} // namespace fringe
