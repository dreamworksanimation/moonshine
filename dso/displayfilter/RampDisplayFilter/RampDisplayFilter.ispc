// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

// Logic similar to RampMap

#include <moonray/rendering/shading/ispc/Util.isph>
#include <moonray/rendering/shading/ispc/RampControl.isph>
#include <moonray/rendering/displayfilter/DisplayFilter.isph>
#include <scene_rdl2/common/math/ispc/Mat4.isph>
#include <scene_rdl2/common/platform/IspcUtil.isph>

// arbitrary max size to limit the number of control pts
enum RampPtsSize {
    MAX_POS_SIZE = 20
};

ISPC_UTIL_EXPORT_ENUM_TO_HEADER(RampPtsSize);

enum InterpolationType {
    NONE = 0,
    LINEAR,
    EXPONENTIAL_UP,
    EXPONENTIAL_DOWN,
    SMOOTH,
    CATMULL_ROM
};

struct RampDisplayFilter
{
    RampInterpolator2DType mRampType;
    ColorRampControl* mRampControl;
    bool mUseMask;
    bool mInvertMask;
    float mMix;
    bool mHasInput;
};

export const uniform RampDisplayFilter * uniform
RampDisplayFilter_get(const uniform DisplayFilter * uniform displayFilter)
{
    return DISPLAYFILTER_GET_ISPC_CPTR(RampDisplayFilter, displayFilter);
}

static void
filter(const uniform DisplayFilter * uniform me,
       const uniform InputBuffer * const uniform * const uniform inputBuffers,
       const varying DisplayFilterState * const uniform state,
       varying Color * uniform result)
{
    const uniform RampDisplayFilter * uniform self = RampDisplayFilter_get(me);

    const uniform RampInterpolator2DType rampType = self->mRampType;
    const int x = state->mOutputPixelX;
    const int y = state->mOutputPixelY;
    const int w = state->mImageWidth;
    const int h = state->mImageHeight;
    const float uvX = x / (float) w;
    const float uvY = y / (float) h;
    const Vec2f uv = Vec2f_ctor(uvX, uvY);
    if (self->mHasInput) {
        const uniform InputBuffer const * uniform inBuffer = inputBuffers[0];
        const varying Color pixel = InputBuffer_getPixel(inBuffer, state->mOutputPixelX, state->mOutputPixelY);

        // mask can only be applied if there's an input
        const float mix = DISPLAYFILTER_mixAndMask(self->mMix,
                                             self->mUseMask ? inputBuffers[1] : nullptr,
                                             state->mOutputPixelX,
                                             state->mOutputPixelY,
                                             self->mInvertMask);
        if (isZero(mix)) {
            *result = pixel;
            return;
        }

        const varying Color pixelClamped = clamp(pixel, 0.f, 1.f);
        varying float inputRamp = (pixelClamped.r + pixelClamped.g + pixelClamped.b) / 3.0;
        *result = ColorRampControl_eval2D(uv, self->mRampType, inputRamp, self->mRampControl);

        if (!isOne(mix)) {
            *result = lerp(pixel, *result, mix);
        }

    } else {
        // if there is no input, pixel is some arbitrary, unused value
        *result = ColorRampControl_eval2D(uv, self->mRampType, 0.0f, self->mRampControl);
    }
}

DEFINE_DISPLAY_FILTER(RampDisplayFilter, filter)

//---------------------------------------------------------------------------

