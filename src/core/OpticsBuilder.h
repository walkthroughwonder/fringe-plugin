#pragma once

#include "FringeTypes.h"

namespace fringe
{

/** Rasterize optical presets into a speed map (1 = free, 0 = wall). */
class OpticsBuilder
{
public:
    static void build (Preset preset, float slit, float slitW, int w, int h, float* speedOut);

private:
    static void fill (float* m, int n, float v);
    static void wallRect (float* m, int w, int h, float x0, float y0, float x1, float y1);
    static void lensBlob (float* m, int w, int h, float cx, float cy, float radius, float ior);
};

} // namespace fringe
