// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include <moonray/rendering/displayfilter/DisplayFilter.isph>
#include <scene_rdl2/common/math/ispc/ColorSpace.isph>

enum RgbToHsvMode {
    RGB_TO_HSV = 0,
    HSV_TO_RGB = 1
};

struct RgbToHsvDisplayFilter
{
    RgbToHsvMode mMode;
};

export const uniform RgbToHsvDisplayFilter * uniform
RgbToHsvDisplayFilter_get(const uniform DisplayFilter * uniform displayFilter)
{
    return DISPLAYFILTER_GET_ISPC_CPTR(RgbToHsvDisplayFilter, displayFilter);
}

static void
filter(const uniform DisplayFilter * uniform me,
       const uniform InputBuffer * const uniform * const uniform inputBuffers,
       const varying DisplayFilterState * const uniform state,
       varying Color * uniform result)
{
    const uniform RgbToHsvDisplayFilter * uniform self = RgbToHsvDisplayFilter_get(me);
    const uniform InputBuffer const * uniform inBuffer = inputBuffers[0];
    const varying Color pixel = InputBuffer_getPixel(inBuffer, state->mOutputPixelX, state->mOutputPixelY);
    
    if (self->mMode == RGB_TO_HSV) {
        *result = rgbToHsv(pixel);
    } else {
        *result = hsvToRgb(pixel);
    }  
}

DEFINE_DISPLAY_FILTER(RgbToHsvDisplayFilter, filter)

//---------------------------------------------------------------------------

