// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include <moonray/rendering/displayfilter/DisplayFilter.isph>

struct ConvolutionDisplayFilter
{
    float* mKernel;
    unsigned int mKernelSize;
    bool  mMask;
    bool  mInvertMask;
    float mMix;
};

export const uniform ConvolutionDisplayFilter * uniform
ConvolutionDisplayFilter_get(const uniform DisplayFilter * uniform displayFilter)
{
    return DISPLAYFILTER_GET_ISPC_CPTR(ConvolutionDisplayFilter, displayFilter);
}

varying Color
convolve(const uniform float * uniform kernel,
         const uniform unsigned int kSizeSqrt, // eg. 3, 5, 7, 9, etc
         varying unsigned int x, varying unsigned int y,
         const uniform InputBuffer const * uniform inBuffer)
{
    const uniform int kRad = (kSizeSqrt -1) / 2;

    varying Color result = Color_ctor(0.0f, 0.0f, 0.0f);

    for (uniform int ky = -kRad, j = 0; ky <= kRad; ++ky, ++j) {
        for (uniform int kx = -kRad, i = 0; kx <= kRad; ++kx, ++i) {
            const uniform float wt = kernel[j * kSizeSqrt + i];
            const varying Color src = InputBuffer_getPixel(inBuffer, x + kx, y + ky);
            result = result + src * wt;
        }
    }
    return result;
}

static void
filter(const uniform DisplayFilter * uniform me,
       const uniform InputBuffer * const uniform * const uniform inputBuffers,
       const varying DisplayFilterState * const uniform state,
       varying Color * uniform result)
{
    const uniform ConvolutionDisplayFilter * uniform self = ConvolutionDisplayFilter_get(me);

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

    *result = convolve(self->mKernel, self->mKernelSize,
                       state->mOutputPixelX, state->mOutputPixelY,
                       inBuffer);

    if (!isOne(mix)) {
        *result = lerp(InputBuffer_getPixel(inBuffer, state->mOutputPixelX, state->mOutputPixelY),
                       *result, mix);
    }
}

DEFINE_DISPLAY_FILTER(ConvolutionDisplayFilter, filter)


//---------------------------------------------------------------------------

