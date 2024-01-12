// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

static Vec3f
sampleNormal(const uniform NormalMap* uniform map,
             uniform ShadingTLState* uniform tls,
             const varying State& state)
{
    const varying Color input = evalAttrInput(map, tls, state);
    varying Vec3f result = Vec3f_ctor(input.r, input.g, input.b);
    return result;
}

DEFINE_NORMALMAP_SHADER(RgbToNormalMap, sampleNormal)

