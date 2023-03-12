// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ColorCorrectTMIMap.ispc

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

    varying Color tmiColor = evalAttrTMI(map, tls, state);
    applyTMI(tmiColor.r, tmiColor.g, tmiColor.b, sample.r, sample.g, sample.b);
    
    if (!isEqual(mix, 1.0f)) {
        sample.r = lerpOpt(input.r, sample.r, mix);
        sample.g = lerpOpt(input.g, sample.g, mix);
        sample.b = lerpOpt(input.b, sample.b, mix);
    }

    return sample;
}

DEFINE_MAP_SHADER(ColorCorrectTMIMap, sample)

// TM and (c) 2019-2022r DreamWorks Animation LLC.  All Rights Reserved.
