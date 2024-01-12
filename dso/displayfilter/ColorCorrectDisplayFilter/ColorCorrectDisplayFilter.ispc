// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include <moonray/rendering/displayfilter/DisplayFilter.isph>

struct ColorCorrectDisplayFilter
{
    float mExposure;
    float mSaturation;
    float mContrast;
    float mGamma;
    Color mOffset;
    Color mMultiply;
    bool  mMask;
    bool  mInvertMask;
    float mMix;
};

export const uniform ColorCorrectDisplayFilter * uniform
ColorCorrectDisplayFilter_get(const uniform DisplayFilter * uniform displayFilter)
{
    return DISPLAYFILTER_GET_ISPC_CPTR(ColorCorrectDisplayFilter, displayFilter);
}

static void
filter(const uniform DisplayFilter * uniform me,
       const uniform InputBuffer * const uniform * const uniform inputBuffers,
       const varying DisplayFilterState * const uniform state,
       varying Color * uniform result)
{
    const uniform ColorCorrectDisplayFilter * uniform self = ColorCorrectDisplayFilter_get(me);
    const uniform InputBuffer const * uniform inBuffer = inputBuffers[0];
    varying Color src = InputBuffer_getPixel(inBuffer, state->mOutputPixelX, state->mOutputPixelY);
    const varying Color original = src;

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

    if (self->mExposure != 0.0) {
        const uniform float exposure = pow(2.0f, self->mExposure);
        src = src * exposure;
    }
    if (self->mSaturation != 1.0f) {
        // computeLuminance
        const float y = src.r * 0.212671f + src.g * 0.715160f + src.b * 0.072169f;
        uniform float saturation = self->mSaturation;
        src.r = lerp(y, src.r, saturation);
        src.g = lerp(y, src.g, saturation);
        src.b = lerp(y, src.b, saturation);
    }
    uniform float contrast = self->mContrast;
    if (contrast != 0.0f) {
        contrast = clamp(contrast, -1.f, 1.f);
        float f = (1.f + contrast) / (1.f - contrast);
        src.r = f * (src.r - 0.5f) + 0.5f;
        src.g = f * (src.g - 0.5f) + 0.5f;
        src.b = f * (src.b - 0.5f) + 0.5f;
    }
    const uniform float gamma = self->mGamma;
    if (gamma != 1.0f && gamma > 0.f) {
        src.r = pow(src.r, 1.f / gamma);
        src.g = pow(src.g, 1.f / gamma);
        src.b = pow(src.b, 1.f / gamma);
    }
    if (!isBlack(self->mOffset)) {
        src = src + self->mOffset;
    }
    if (!isWhite(self->mMultiply)) {
        src = src * self->mMultiply;
    }

    *result = src;

    if (!isOne(mix)) {
        *result = lerp(original, *result, mix);
    }
}

DEFINE_DISPLAY_FILTER(ColorCorrectDisplayFilter, filter)

//---------------------------------------------------------------------------

