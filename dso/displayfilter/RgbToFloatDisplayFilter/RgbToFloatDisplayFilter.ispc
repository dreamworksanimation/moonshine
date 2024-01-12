// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include <moonray/rendering/displayfilter/DisplayFilter.isph>

enum RgbToFloatMode {
    R = 0,
    G = 1,
    B = 2,
    MIN = 3,
    MAX = 4,
    AVERAGE = 5,
    SUM = 6,
    LUMINANCE = 7
};

struct RgbToFloatDisplayFilter
{
    RgbToFloatMode mMode;
    bool  mMask;
    bool  mInvertMask;
    float mMix;
};

export const uniform RgbToFloatDisplayFilter * uniform
RgbToFloatDisplayFilter_get(const uniform DisplayFilter * uniform displayFilter)
{
    return DISPLAYFILTER_GET_ISPC_CPTR(RgbToFloatDisplayFilter, displayFilter);
}

float rgbToFloat(RgbToFloatMode mode, const varying Color& rgb)
{

    switch (mode) {
    case R:
        return rgb.r;
    case G:
        return rgb.g;
    case B:
        return rgb.b;
    case MIN:
        return min(min(rgb.r, rgb.g), rgb.b);
    case MAX:
        return max(max(rgb.r, rgb.g), rgb.b);
    case AVERAGE:
        return (rgb.r + rgb.g + rgb.b) / 3.0f;
    case SUM:
        return rgb.r + rgb.g + rgb.b;
    case LUMINANCE:
        return luminance(rgb);
    default:
        return 0.f;
    }
}

static void
filter(const uniform DisplayFilter * uniform me,
       const uniform InputBuffer * const uniform * const uniform inputBuffers,
       const varying DisplayFilterState * const uniform state,
       varying Color * uniform result)
{
    const uniform RgbToFloatDisplayFilter * uniform self = RgbToFloatDisplayFilter_get(me);

    // input buffers given to us are in the same order as the
    // inputs SceneObjects we returned from getInputData();
    const uniform InputBuffer const * uniform inBuffer = inputBuffers[0];
    const varying Color src = InputBuffer_getPixel(inBuffer,
                                                   state->mOutputPixelX,
                                                   state->mOutputPixelY);

    // first check if mix is 0
    float mix = DISPLAYFILTER_mixAndMask(self->mMix,
                                         self->mMask ? inputBuffers[1] : nullptr,
                                         state->mOutputPixelX,
                                         state->mOutputPixelY,
                                         self->mInvertMask);
    if (isZero(mix)) {
        *result = src;
        return;
    }

    const float f = rgbToFloat(self->mMode, src);
    result->r = f;
    result->g = f;
    result->b = f;

    if (!isOne(mix)) {
        *result = lerp(src, *result, mix);
    }
}

DEFINE_DISPLAY_FILTER(RgbToFloatDisplayFilter, filter)

//---------------------------------------------------------------------------

