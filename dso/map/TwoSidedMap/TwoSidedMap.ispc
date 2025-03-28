// Copyright 2025 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file TwoSidedMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

static Color
sample(const uniform Map *uniform map,
       uniform ShadingTLState * uniform tls,
       const varying State &state)
{
    if (isEntering(state)) {
        return evalAttrFrontColor(map, tls, state);
    } else {
        return evalAttrBackColor(map, tls, state);
    }
}

DEFINE_MAP_SHADER(TwoSidedMap, sample)

