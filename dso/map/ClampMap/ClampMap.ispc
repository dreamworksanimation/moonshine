// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ClampMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

static Color
sample(const uniform Map *            uniform map,
             uniform ShadingTLState * uniform tls,
       const varying State &                  state)
{
    const varying Color input = evalAttrInput(map, tls, state);
    varying float result[3] = {input.r, input.g, input.b};

    if (getAttrClamp(map)) {
        for (uniform int i = 0; i < 3; ++i) {
            result[i] = clamp(result[i], getAttrClampMin(map), getAttrClampMax(map));
        }
    }

    return Color_ctor(result[0], result[1], result[2]);
}
DEFINE_MAP_SHADER(ClampMap, sample)

