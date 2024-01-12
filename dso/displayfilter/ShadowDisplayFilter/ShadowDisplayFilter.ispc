// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include <moonray/rendering/displayfilter/DisplayFilter.isph>

struct ShadowDisplayFilter
{
    float mDensity;
    Color mShadowColor;
};

export const uniform ShadowDisplayFilter * uniform
ShadowDisplayFilter_get(const uniform DisplayFilter * uniform displayFilter)
{
    return DISPLAYFILTER_GET_ISPC_CPTR(ShadowDisplayFilter, displayFilter);
}

static void
filter(const uniform DisplayFilter * uniform me,
       const uniform InputBuffer * const uniform * const uniform inputBuffers,
       const varying DisplayFilterState * const uniform state,
       varying Color * uniform result)
{
    const uniform ShadowDisplayFilter * uniform self = ShadowDisplayFilter_get(me);

    // get user attributes
    uniform float density = self->mDensity;
    uniform Color shadowColor = self->mShadowColor;

    // get input image buffers
    const uniform InputBuffer const * uniform occludedBuffer = inputBuffers[0]; //occluded
    const uniform InputBuffer const * uniform unoccludedBuffer = inputBuffers[1]; // unoccluded

    // get pixel coordinates
    varying int x = state->mOutputPixelX;
    varying int y = state->mOutputPixelY;

    // get input pixels
    varying Color occludedPixel = InputBuffer_getPixel(occludedBuffer, x, y);
    varying Color unoccludedPixel = InputBuffer_getPixel(unoccludedBuffer, x, y);

    Color diff = unoccludedPixel - occludedPixel;
    Color coloredOccluded = occludedPixel + diff * shadowColor;
    Color final = lerp(unoccludedPixel, coloredOccluded, density);

    *result = final;
}

DEFINE_DISPLAY_FILTER(ShadowDisplayFilter, filter)


//---------------------------------------------------------------------------

