// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include <moonray/rendering/displayfilter/DisplayFilter.isph>

struct DiscretizeDisplayFilter
{
    unsigned int mNumBins;
    bool  mMask;
    bool  mInvertMask;
    float mMix;
};

export const uniform DiscretizeDisplayFilter * uniform
DiscretizeDisplayFilter_get(const uniform DisplayFilter * uniform displayFilter)
{
    return DISPLAYFILTER_GET_ISPC_CPTR(DiscretizeDisplayFilter, displayFilter);
}

static void
filter(const uniform DisplayFilter * uniform me,
       const uniform InputBuffer * const uniform * const uniform inputBuffers,
       const varying DisplayFilterState * const uniform state,
       varying Color * uniform result)
{
    const uniform DiscretizeDisplayFilter * uniform self = DiscretizeDisplayFilter_get(me);
    const uniform InputBuffer const * uniform inBuffer = inputBuffers[0];
    const varying Color src = InputBuffer_getPixel(inBuffer, state->mOutputPixelX, state->mOutputPixelY);

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

    const uniform int numBins = self->mNumBins;
    result->r = ceil(src.r * numBins) / numBins;
    result->g = ceil(src.g * numBins) / numBins;
    result->b = ceil(src.b * numBins) / numBins;

    if (!isOne(mix)) {
        *result = lerp(src, *result, mix);
    }
}

DEFINE_DISPLAY_FILTER(DiscretizeDisplayFilter, filter)

//---------------------------------------------------------------------------

