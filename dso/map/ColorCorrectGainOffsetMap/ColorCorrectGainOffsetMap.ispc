// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ColorCorrectGainOffsetMap.ispc

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

    varying Color gain;
    varying Color offset;
    if (getAttrGainOffsetPerChannel(map)) {
        gain = Color_ctor(evalAttrGainR(map, tls, state),
                          evalAttrGainG(map, tls, state),
                          evalAttrGainB(map, tls, state));
        offset = Color_ctor(evalAttrOffsetR(map, tls, state),
                            evalAttrOffsetG(map, tls, state),
                            evalAttrOffsetB(map, tls, state));
    } else {
        const float gainFloat = evalAttrGain(map, tls, state);
        const float offsetFloat = evalAttrOffset(map, tls, state);
        gain = Color_ctor(gainFloat);
        offset = Color_ctor(offsetFloat);
    }

    applyGainAndOffset( gain.r, gain.g, gain.b,
                        offset.r, offset.g, offset.b,
                        sample.r, sample.g, sample.b);

    if (!isEqual(mix, 1.0f)) {
        sample.r = lerpOpt(input.r, sample.r, mix);
        sample.g = lerpOpt(input.g, sample.g, mix);
        sample.b = lerpOpt(input.b, sample.b, mix);
    }

    return sample;
}

DEFINE_MAP_SHADER(ColorCorrectGainOffsetMap, sample)

