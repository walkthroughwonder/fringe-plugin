#pragma once

#include "FringeTypes.h"

namespace fringe
{

class OpticsBuilder
{
public:
    static void build (Preset preset, float slit, float slitW, int w, int h, float* speedOut);

    /** Paint a circular wall (0) or erase (1) brush into the speed map. */
    static void paintDot (float* speed, int w, int h, float uvX, float uvY, float radiusUv, bool erase);

private:
    static void fill (float* m, int n, float v);
    static void wallRect (float* m, int w, int h, float x0, float y0, float x1, float y1);
    static void wallSegment (float* m, int w, int h, float x0, float y0, float x1, float y1, float thickness);
    static void lensBlob (float* m, int w, int h, float cx, float cy, float radius, float ior);
    static void beamsplitter (float* m, int w, int h, float cx, float cy, float angle, float length, float thickness, float ior);
};

} // namespace fringe
