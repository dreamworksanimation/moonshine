// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

// based on code from RemapMap

#include <moonray/rendering/displayfilter/DisplayFilter.isph>

enum RemapMethod {
    REMAP_UNIFORM = 0,
    REMAP_RGB = 1
};

struct RemapDisplayFilter
{
    uniform float mInMin;
    uniform float mInMax;
    uniform float mOutMin;
    uniform float mOutMax;
    uniform float mBiasAmount;

    uniform Color mInMinRGB;
    uniform Color mInMaxRGB;
    uniform Color mOutMinRGB;
    uniform Color mOutMaxRGB;
    uniform Color mBiasAmountRGB;
    uniform RemapMethod mRemapMethod;

    uniform bool mClamp;
    uniform float mClampMin;
    uniform float mClampMax;
    uniform bool mClampRGB;
    uniform Color mClampRGBMin;
    uniform Color mClampRGBMax;

    bool  mUseMask;
    bool  mInvertMask;
    float mMix;
};

export const uniform RemapDisplayFilter * uniform
RemapDisplayFilter_get(const uniform DisplayFilter * uniform displayFilter)
{
    return DISPLAYFILTER_GET_ISPC_CPTR(RemapDisplayFilter, displayFilter);
}

/** Remaps each individual channel of the given pixel color
 *  @param inMin, inMax - the low and high limits of the input range for each channel
 *  @param outMin, outMax - the low and high limits of the output range for each channel
 *  @param applyBias - whether to apply bias to each channel (true if biasAmount is not 0.5)
 *  @param biasAmount - amount of bias, where 0.5 represents no bias
 */
static varying Color
remapRGB(const varying Color& pixel,
             const uniform Color inMin, const uniform Color inMax,
             const uniform Color outMin, const uniform Color outMax,
             const uniform bool applyBias[3],
             const uniform Color biasAmount)
{
    const uniform Color inRange = inMax - inMin;
    const uniform Color outRange = outMax - outMin;

    // map pixel color to the [inMin, inMax] range
    varying float tr = (pixel.r - inMin.r) / inRange.r;
    varying float tg = (pixel.g - inMin.g) / inRange.g;
    varying float tb = (pixel.b - inMin.b) / inRange.b;

    // apply bias
    tr = applyBias[0] ? bias(tr, biasAmount.r) : tr;
    tg = applyBias[1] ? bias(tg, biasAmount.g) : tg;
    tb = applyBias[2] ? bias(tb, biasAmount.b) : tb;

    // map t to [outMin, outMax] range
    varying Color result = pixel;
    result.r = tr * outRange.r + outMin.r;
    result.g = tg * outRange.g + outMin.g;
    result.b = tb * outRange.b + outMin.b;

    return result;
}

/** Remaps the given pixel color uniformly across the RGB components
 *  @param inMin, inMax - the low and high limits of the input range
 *  @param outMin, outMax - the low and high limits of the output range
 *  @param applyBias - whether to apply bias (true if biasAmount is not 0.5)
 *  @param biasAmount - amount of bias, where 0.5 represents no bias
 */
static varying Color
remapUniform(const varying Color& pixel,
             const uniform float inMin, const uniform float inMax,
             const uniform float outMin, const uniform float outMax,
             const uniform bool applyBias,
             const uniform float biasAmount)
{
    const uniform float inRange = inMax - inMin;
    const uniform float outRange = outMax - outMin;

    varying float result[3] = {pixel.r, pixel.g, pixel.b};
    for (uniform int i = 0; i < 3; ++i) {
        // map pixel color to [inMin, inMax] range
        float t = (result[i]-inMin) / inRange;

        if (applyBias) {
            t = bias(t, biasAmount);
        }
        // map t value to [outMin, outMax] range
        result[i] = t * outRange + outMin;
    }
    return Color_ctor(result[0], result[1], result[2]);
}

static void
filter(const uniform DisplayFilter * uniform me,
       const uniform InputBuffer * const uniform * const uniform inputBuffers,
       const varying DisplayFilterState * const uniform state,
       varying Color * uniform result)
{
    const uniform RemapDisplayFilter * uniform self = RemapDisplayFilter_get(me);
    const uniform InputBuffer const * uniform inBuffer = inputBuffers[0];
    varying Color src = InputBuffer_getPixel(inBuffer, state->mOutputPixelX, state->mOutputPixelY);

    *result = src;

    float mix = DISPLAYFILTER_mixAndMask(self->mMix,
                                         self->mUseMask ? inputBuffers[1] : nullptr,
                                         state->mOutputPixelX,
                                         state->mOutputPixelY,
                                         self->mInvertMask);
    if (isZero(mix)) {
        *result = src;
        return;
    }

    switch (self->mRemapMethod) {
        case REMAP_UNIFORM:
        {
            const uniform bool applyBias = !isEqual(self->mBiasAmount, 0.5f);

            // if the ranges are different, remapping must be performed
            if (!isEqual(self->mInMin, self->mOutMin) || !isEqual(self->mInMax, self->mOutMax)) {
                // apply remapping and potentially bias 
                *result = remapUniform(src, self->mInMin, self->mInMax, self->mOutMin, 
                                       self->mOutMax, applyBias, self->mBiasAmount);
            } else if (applyBias) {
                result->r = bias(src.r, self->mBiasAmount);
                result->g = bias(src.g, self->mBiasAmount);
                result->b = bias(src.b, self->mBiasAmount);
            }

            if (self->mClamp) {
                result->r = clamp(result->r, self->mClampMin, self->mClampMax);
                result->g = clamp(result->g, self->mClampMin, self->mClampMax);
                result->b = clamp(result->b, self->mClampMin, self->mClampMax);
            }
        }
        break;
        case REMAP_RGB:
        {
            const uniform bool applyBiasRGB[3] = {!isEqual(self->mBiasAmountRGB.r, 0.5f), 
                                                  !isEqual(self->mBiasAmountRGB.g, 0.5f), 
                                                  !isEqual(self->mBiasAmountRGB.b, 0.5f)};

            if (!isEqual(self->mInMinRGB, self->mOutMinRGB) || !isEqual(self->mInMaxRGB, self->mOutMaxRGB)) {
                // apply remapping and potentially bias
                *result = remapRGB(src, self->mInMinRGB, self->mInMaxRGB, self->mOutMinRGB, 
                                   self->mOutMaxRGB, applyBiasRGB, self->mBiasAmountRGB);
            } else {
                // if ranges are the same, just apply bias
                result->r = applyBiasRGB[0] ? bias(src.r, self->mBiasAmountRGB.r) : src.r;
                result->g = applyBiasRGB[1] ? bias(src.g, self->mBiasAmountRGB.g) : src.g;
                result->b = applyBiasRGB[2] ? bias(src.b, self->mBiasAmountRGB.b) : src.b;
            }

            if (self->mClampRGB){
                result->r = clamp(result->r, self->mClampRGBMin.r, self->mClampRGBMax.r);
                result->g = clamp(result->g, self->mClampRGBMin.g, self->mClampRGBMax.g);
                result->b = clamp(result->b, self->mClampRGBMin.b, self->mClampRGBMax.b);
            }
        }
        break;
        default:
            break;
    }

    if (!isOne(mix)) {
        *result = lerp(src, *result, mix);
    }
}

DEFINE_DISPLAY_FILTER(RemapDisplayFilter, filter)

//---------------------------------------------------------------------------

