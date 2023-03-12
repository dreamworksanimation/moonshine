// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ColorCorrectSaturationMap.ispc

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

    varying Color saturation;
    if (getAttrSaturationPerChannel(map)) {
        saturation = Color_ctor(evalAttrSaturationR(map, tls, state),
                                evalAttrSaturationG(map, tls, state),
                                evalAttrSaturationB(map, tls, state));
    } else {
        const float saturationFloat = evalAttrSaturation(map, tls, state);
        saturation = Color_ctor(saturationFloat);
    }
    applySaturation(saturation.r, saturation.g, saturation.b,
                    sample.r, sample.g, sample.b);

    if (!isEqual(mix, 1.0f)) {
        sample.r = lerpOpt(input.r, sample.r, mix);
        sample.g = lerpOpt(input.g, sample.g, mix);
        sample.b = lerpOpt(input.b, sample.b, mix);
    }

    return sample;
}

DEFINE_MAP_SHADER(ColorCorrectSaturationMap, sample)

