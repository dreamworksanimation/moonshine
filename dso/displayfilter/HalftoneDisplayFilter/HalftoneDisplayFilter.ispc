// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include <moonray/rendering/displayfilter/DisplayFilter.isph>

struct HalftoneDisplayFilter
{
    int mSize;
    float mFilterWidth;
    bool mInvert;
    bool mGrayscale;
    bool  mMask;
    bool  mInvertMask;
    float mMix;
};

export const uniform HalftoneDisplayFilter * uniform
HalftoneDisplayFilter_get(const uniform DisplayFilter * uniform displayFilter)
{
    return DISPLAYFILTER_GET_ISPC_CPTR(HalftoneDisplayFilter, displayFilter);
}

float
smoothstep(const float edge0, const float edge1,
           const float t)
{
    const float s = saturate((t - edge0) / (edge1 - edge0));
    return s * s * (3 - 2 * s);
}

// compute the appropriate radius within a unit square for a
// black halftone dot so that the remaining uncovered area
// is equal to the given value.  In other words, if the
// value of a region is 0.2 (as seen from afar), the dot
// should cover 80% of the unit square. What would that
// dot's radius be ?
float
computeDotRadius(const varying float value)
{
    // how big should the black dot be so that the uncovered
    // area is equal to the passed value argument ?
    //
    // #####################
    // ######.........######  // white unit square has area 1.0
    // ####.............####
    // ###...............###  // block dot in center has area
    // ##.................##  // A = 1.0 - value
    // ###...............###
    // ####.............####  // remaining white area = value
    // ######.........######
    // #####################
    // A = 1 - value
    // pi * r^2 = 1 - value
    // r^2 = (1 - value)/pi
    const float rSqr = (1.0f - saturate(value)) * sOneOverPi;
    return sqrt(rSqr);
}

varying Vec3f
cellAverage(const uniform InputBuffer * const uniform buf,
            const uniform int cellSize,
            varying unsigned int originX,
            varying unsigned int originY,
            varying unsigned int imageWidth,
            varying unsigned int imageHeight)
{
    Vec3f result = Vec3f_ctor(0.f, 0.f, 0.f);

    uniform unsigned int count = 0;
    for (uniform int y = -cellSize; y <= cellSize; ++y) {
        int yy = originY + y;
        if (yy >= imageHeight || yy < 0) continue;
        for (uniform int x = -cellSize; x <= cellSize; ++x) {
            int xx = originX + x;
            if (xx >= imageWidth || xx < 0) continue;

            const Vec3f pixel = InputBuffer_getFloat3Pixel(buf, xx, yy);
            result.x += pixel.x;
            result.y += pixel.y;
            result.z += pixel.z;
            ++count;
        }
    }

    const float denom = 1.0f / count;
    return result * denom;
}

static void
filter(const uniform DisplayFilter * uniform me,
       const uniform InputBuffer * const uniform * const uniform inputBuffers,
       const varying DisplayFilterState * const uniform state,
       varying Color * uniform result)
{
    const uniform HalftoneDisplayFilter * uniform self = HalftoneDisplayFilter_get(me);

    // input buffers given to us are in the same order as the
    // inputs SceneObjects we returned from getInputData();
    const uniform InputBuffer const * uniform inBuffer = inputBuffers[0];

    // first check if mix is 0
    float mix = DISPLAYFILTER_mixAndMask(self->mMix,
                                         self->mMask ? inputBuffers[1] : nullptr,
                                         state->mOutputPixelX,
                                         state->mOutputPixelY,
                                         self->mInvertMask);
    if (isZero(mix)) {
        *result = InputBuffer_getPixel(inBuffer, state->mOutputPixelX, state->mOutputPixelY);
        return;
    }

    const uniform unsigned int cellSize = self->mSize;
    const uniform float scale = 1.f / (float)cellSize;
    const uniform float filterWidth = self->mFilterWidth;
    const uniform bool invert = self->mInvert;
    const uniform bool grayscale = self->mGrayscale;

    // determine which grid cell we are in
    int x = state->mOutputPixelX;
    int y = state->mOutputPixelY;
    size_t cellX = x / cellSize;
    size_t cellY = y / cellSize;

    // measure distance between x,y and cell center.
    // lx, ly are the pixel coordinates local to the cell.
    // xx, yy are the coordinates within unit square.
    const unsigned int lx = x - (cellX * cellSize); // modulo
    const unsigned int ly = y - (cellY * cellSize); // modulo

    // transform from pixels to [0,1] (cell-space),
    const float xx = lx * scale;
    const float yy = ly * scale;

    // From now on all values are [0,1] (cell-space), not pixels
    const float deltaX = (xx - 0.5f); // from center
    const float deltaY = (yy - 0.5f); // from center

    // distance to center of cell/dot
    const float dist = sqrt(deltaX*deltaX + deltaY*deltaY);

    const Vec3f average = cellAverage(inBuffer, cellSize,
                                      state->mOutputPixelX, state->mOutputPixelY,
                                      state->mImageWidth, state->mImageHeight);

    // We use the max component (aka. "value") to compute a halftone
    // dot radius that gives the same "tone" when seen from afar. The cell
    // will be white with a black dot, or black with a white dot if 'invert'
    // is checked.
    float tone = saturate(reduce_max(average));
    if (invert) tone = 1.0f - tone;

    const float radius = computeDotRadius(tone);

    // use smoothstep function to draw antialiased dots
    float dotPattern = smoothstep(radius - filterWidth, radius + filterWidth, dist);
    if (invert) dotPattern = 1.0f - dotPattern;

    if (grayscale) {
        *result = Color_ctor(dotPattern, dotPattern, dotPattern);
    } else {
        *result = Color_ctor(average.x * dotPattern, average.y * dotPattern, average.z * dotPattern);
    }

    if (!isOne(mix)) {
        *result = lerp(InputBuffer_getPixel(inBuffer, state->mOutputPixelX, state->mOutputPixelY),
                       *result, mix);
    }
}

DEFINE_DISPLAY_FILTER(HalftoneDisplayFilter, filter)

//---------------------------------------------------------------------------

