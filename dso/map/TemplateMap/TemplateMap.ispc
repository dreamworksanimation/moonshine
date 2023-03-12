// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file TemplateMap.ispc

// auto-generated
#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

// this sample function returns the color sampled at state
static Color
sample(const uniform Map * uniform map,
       uniform ShadingTLState * uniform tls,
       const varying State &state)
{
    // return the color value of the attribute * any bound map result
    return evalAttrColor(map, state);
}

DEFINE_MAP_SHADER(TemplateMap, sample)

