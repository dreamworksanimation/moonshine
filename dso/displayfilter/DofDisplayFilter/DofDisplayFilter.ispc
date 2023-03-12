// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include <moonray/rendering/displayfilter/DisplayFilter.isph>

struct DofDisplayFilter
{
    float mAperture;
    float mFocalLength;
    float mFocusDistance;
    bool  mMask;
    bool  mInvertMask;
    float mMix;
};

export const uniform DofDisplayFilter * uniform
DofDisplayFilter_get(const uniform DisplayFilter * uniform displayFilter)
{
    return DISPLAYFILTER_GET_ISPC_CPTR(DofDisplayFilter, displayFilter);
}

static void
filter(const uniform DisplayFilter * uniform me,
       const uniform InputBuffer * const uniform * const uniform inputBuffers,
       const varying DisplayFilterState * const uniform state,
       varying Color * uniform result)
{
    const uniform DofDisplayFilter * uniform self = DofDisplayFilter_get(me);

    // input buffers given to us are in the same order as the
    // inputs SceneObjects we returned from getInputData();
    const uniform InputBuffer const * uniform inBuffer = inputBuffers[0];

    // first check if mix is 0
    float mix = DISPLAYFILTER_mixAndMask(self->mMix,
                                         self->mMask ? inputBuffers[2] : nullptr,
                                         state->mOutputPixelX,
                                         state->mOutputPixelY,
                                         self->mInvertMask);
    if (isZero(mix)) {
        *result = InputBuffer_getPixel(inBuffer, state->mOutputPixelX, state->mOutputPixelY);
        return;
    }

    const uniform InputBuffer const * uniform depthBuffer = inputBuffers[1];
    const float pixelDepth = InputBuffer_getFloatPixel(depthBuffer,
                                                       state->mOutputPixelX,
                                                       state->mOutputPixelY);

    if (!isinf(pixelDepth)) {
        int width = state->mImageWidth;
        int height = state->mImageHeight;
        // compute a circle of confusion at this depth point.  See:
        // https://developer.nvidia.com/sites/all/modules/custom/gpugems/books/GPUGems/gpugems_ch23.html
        const float coc = abs(self->mAperture * (self->mFocalLength * (pixelDepth - self->mFocusDistance)) /
            (pixelDepth * (self->mFocusDistance - self->mFocalLength)));

        // Convert coc to pixels for our blur radiaus
        const int cocPixels = max(1.f, floor(width > height ? coc * width : coc * height));

        // Box filter because its fast(ish) and easy
        *result = Color_ctor(0.f, 0.f, 0.f);
        int weight = 0;
        for (int filterRow = 0; filterRow < cocPixels; ++filterRow) {
            int srcImageY = filterRow + state->mOutputPixelY - cocPixels / 2;
            if (srcImageY < 0) srcImageY = 0;
            if (srcImageY >= height) srcImageY = height - 1;
            for (int filterCol = 0; filterCol < cocPixels; ++filterCol) {
                int srcImageX = filterCol + state->mOutputPixelX - cocPixels / 2;
                if (srcImageX < 0) srcImageX = 0;
                if (srcImageX >= width) srcImageX = width - 1;

                const varying Vec3f srcPixel = InputBuffer_getFloat3Pixel(inBuffer, srcImageX, srcImageY);
                result->r += srcPixel.x;
                result->g += srcPixel.y;
                result->b += srcPixel.z;
                ++weight;
            }
        }
        if (weight > 0) {
            result->r /= (float)weight;
            result->g /= (float)weight;
            result->b /= (float)weight;
        } else {
            // something is odd, just use the original src
            const varying Vec3f src = InputBuffer_getFloat3Pixel(inBuffer,
                                                                 state->mOutputPixelX,
                                                                 state->mOutputPixelY);
            *result = Color_ctor(src.x, src.y, src.z);
        }

        if (!isOne(mix)) {
           *result = lerp(InputBuffer_getPixel(inBuffer, state->mOutputPixelX, state->mOutputPixelY),
                         *result, mix);
        }

    } else {
        // no depth info implies no geometry?
        const varying Vec3f src = InputBuffer_getFloat3Pixel(inBuffer,
                                                             state->mOutputPixelX,
                                                             state->mOutputPixelY);
        *result = Color_ctor(src.x, src.y, src.z);
    }
}

DEFINE_DISPLAY_FILTER(DofDisplayFilter, filter)

//---------------------------------------------------------------------------

