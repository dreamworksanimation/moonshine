// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include <moonray/rendering/displayfilter/DisplayFilter.isph>

enum BlendType {
    LINEAR =    0,
    CUBIC =     1
};

struct BlendDisplayFilter
{
    float mBlendAmt;
    BlendType mBlendType;
    bool  mMask;
    bool  mInvertMask;
    float mMix;
};

export const uniform BlendDisplayFilter * uniform
BlendDisplayFilter_get(const uniform DisplayFilter * uniform displayFilter)
{
    return DISPLAYFILTER_GET_ISPC_CPTR(BlendDisplayFilter, displayFilter);
}

static void
filter(const uniform DisplayFilter * uniform me,
       const uniform InputBuffer * const uniform * const uniform inputBuffers,
       const varying DisplayFilterState * const uniform state,
       varying Color * uniform result)
{
    const uniform BlendDisplayFilter * uniform self = BlendDisplayFilter_get(me);

    // input buffers given to us are in the same order as the
    // inputs SceneObjects we returned from getInputData();
    const uniform InputBuffer const * uniform inBuffer0 = inputBuffers[0];
    const varying Color pixelA = InputBuffer_getPixel(inBuffer0,
                                                      state->mOutputPixelX,
                                                      state->mOutputPixelY);

    const uniform InputBuffer const * uniform inBuffer1 = inputBuffers[1];
    const varying Color pixelB = InputBuffer_getPixel(inBuffer1,
                                                      state->mOutputPixelX,
                                                      state->mOutputPixelY);

    float blendAmt;
    const uniform float t = self->mBlendAmt;
    const uniform BlendType blendType = self->mBlendType;

    if (blendType == LINEAR) {
        blendAmt = t;
    } else {
        blendAmt = t * t * (3 - 2 * t);
    }
    *result = lerp(pixelA, pixelB, blendAmt);

    // apply mask if necessary
    float mix = DISPLAYFILTER_mixAndMask(self->mMix,
                                         self->mMask ? inputBuffers[2] : nullptr,
                                         state->mOutputPixelX,
                                         state->mOutputPixelY,
                                         self->mInvertMask);
    if (isZero(mix)) {
        *result = pixelA;
        return;
    }

    if (!isOne(mix)) {
        *result = lerp(pixelA, *result, mix);
    }
}

DEFINE_DISPLAY_FILTER(BlendDisplayFilter, filter)

//---------------------------------------------------------------------------

