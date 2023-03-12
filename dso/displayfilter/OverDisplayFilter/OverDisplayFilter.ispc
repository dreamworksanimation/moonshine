// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include <moonray/rendering/displayfilter/DisplayFilter.isph>

struct OverDisplayFilter
{
    bool  mInvertAlpha;
    float mMix;
};

export const uniform OverDisplayFilter * uniform
OverDisplayFilter_get(const uniform DisplayFilter * uniform displayFilter)
{
    return DISPLAYFILTER_GET_ISPC_CPTR(OverDisplayFilter, displayFilter);
}

static void
filter(const uniform DisplayFilter * uniform me,
       const uniform InputBuffer * const uniform * const uniform inputBuffers,
       const varying DisplayFilterState * const uniform state,
       varying Color * uniform result)
{
    const uniform OverDisplayFilter * uniform self = OverDisplayFilter_get(me);

    // input buffers given to us are in the same order as the
    // inputs SceneObjects we returned from getInputData();
    const uniform InputBuffer const * uniform inBottomBuffer = inputBuffers[1];
    const varying Color bottom = InputBuffer_getPixel(inBottomBuffer,
                                                      state->mOutputPixelX,
                                                      state->mOutputPixelY);

    // first check if alpha is 0
    float alpha = DISPLAYFILTER_mixAndMask(self->mMix,
                                           // alpha buffer
                                           inputBuffers[2],
                                           state->mOutputPixelX,
                                           state->mOutputPixelY,
                                           self->mInvertAlpha);

    if (isZero(alpha)) {
        *result = bottom;
        return;
    }

    const uniform InputBuffer const * uniform inTopBuffer = inputBuffers[0];
    const varying Color top = InputBuffer_getPixel(inTopBuffer,
                                                   state->mOutputPixelX,
                                                   state->mOutputPixelY);

    *result = (top - bottom) * alpha + bottom;
}

DEFINE_DISPLAY_FILTER(OverDisplayFilter, filter)

//---------------------------------------------------------------------------

