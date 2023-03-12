// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ConstantColorMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

static Color
sample(const uniform Map *uniform map,
       uniform ShadingTLState * uniform tls,
       const varying State &state)
{
    const uniform Color color = getAttrColorValue(map);
    Color sample = Color_ctor(color.r,
                              color.g,
                              color.b);
    return sample;
}

DEFINE_MAP_SHADER(ConstantColorMap, sample)

