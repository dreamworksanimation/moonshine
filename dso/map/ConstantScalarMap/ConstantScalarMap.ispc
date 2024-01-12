// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ConstantScalarMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

static Color
sample(const uniform Map *uniform map,
       uniform ShadingTLState * uniform tls,
       const varying State &state)
{
    const uniform float scalar = getAttrScalarValue(map);
    Color sample = Color_ctor(scalar);
    return sample;
}

DEFINE_MAP_SHADER(ConstantScalarMap, sample)

