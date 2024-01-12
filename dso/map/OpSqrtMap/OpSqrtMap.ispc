// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file OpSqrtMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

struct OpSqrtMap
{
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(OpSqrtMap);

// temporary, vectorized sampling not yet implemented
static Color
sample(const uniform Map *uniform map,
       uniform ShadingTLState * uniform tls,
       const varying State &state)
{
    // const uniform OpSqrtMap * uniform me = MAP_GET_ISPC_PTR(OpSqrtMap, map);

    const varying Color black = Color_ctor(0.f);
    return black;
}

DEFINE_MAP_SHADER(OpSqrtMap, sample)

