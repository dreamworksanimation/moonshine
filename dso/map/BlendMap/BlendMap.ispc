// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file BlendMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

enum BlendType {
    LINEAR = 0,
    CUBIC
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(BlendType);

static Color
sample(const uniform Map* uniform map,
       uniform ShadingTLState* uniform tls,
       const varying State& state)
{
    varying Color sample;

    const varying float blendAmount = evalAttrBlendAmount(map, tls, state);
    const varying float thresholdMin = evalAttrThresholdMin(map, tls, state);
    const varying float thresholdMax = evalAttrThresholdMax(map, tls, state);
    const uniform int blendType = getAttrBlendType(map);

    if (blendAmount <= thresholdMin) {
        sample = evalAttrColorA(map, tls, state);
    } else if (blendAmount >= thresholdMax) {
        sample = evalAttrColorB(map, tls, state);
    } else {
        const varying Color colorA = evalAttrColorA(map, tls, state);
        const varying Color colorB = evalAttrColorB(map, tls, state);
        const float divisor = (thresholdMax - thresholdMin);

        varying float t = 0.0f;

        if (divisor > 0.0f)
            t = (blendAmount - thresholdMin) * rcp(divisor);

        if (blendType == CUBIC) {
            t = t * t * (3 - 2 * t);
        }
        sample = lerp(colorA, colorB, t);
    }

    return sample;
}

DEFINE_MAP_SHADER(BlendMap, sample)

