// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ColorCorrectContrastMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>
#include <moonray/rendering/shading/ispc/ColorCorrect.isph>

static Color
sample(const uniform Map* uniform map,
       uniform ShadingTLState* uniform tls,
       const varying State& state)
{
    const varying Color input = evalAttrInput(map, tls, state);
    varying Color sample = input;

    const varying float mix = evalAttrMix(map, tls, state);
    if (!getAttrOn(map) || isZero(mix)) {
        return sample;
    }

    varying Color contrast;
    if (getAttrContrastPerChannel(map)) {
        contrast = Color_ctor(evalAttrContrastR(map, tls, state),
                              evalAttrContrastG(map, tls, state),
                              evalAttrContrastB(map, tls, state));
    } else {
        const float contrastFloat = evalAttrContrast(map, tls, state);
        contrast = Color_ctor(contrastFloat);
    }
    applyContrast(contrast, sample);

    if (!isEqual(mix, 1.0f)) {
        sample.r = lerpOpt(input.r, sample.r, mix);
        sample.g = lerpOpt(input.g, sample.g, mix);
        sample.b = lerpOpt(input.b, sample.b, mix);
    }

    return sample;
}

DEFINE_MAP_SHADER(ColorCorrectContrastMap, sample)

