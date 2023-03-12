// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file HairMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

static Color
sample(const uniform Map *           uniform map,
             uniform ShadingTLState *uniform tls,
       const varying State &                 state)
{
    Color sample;

    Vec2f st = state.mSt;
    float s = st.x;

    // Bias: we use 1.0 - bias so small bias values push colors towards root
    s = bias(s, 1.0f - evalAttrLengthWiseBias(map, tls, state));

    // Gain
    s = gain(s, evalAttrLengthWiseGain(map, tls, state));

    // length wise color interpolation
    Color baseColor = evalAttrBaseHairColor(map, tls, state);
    Color tipColor = evalAttrTipHairColor(map, tls, state);
    Color columnUvColor = evalAttrHairColumnUvColor(map, tls, state);

    sample = (baseColor * (1.0f - s) + tipColor * s) * columnUvColor;
    
    return sample;
}

DEFINE_MAP_SHADER(HairMap, sample)
               
