// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file FloatToRgbMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

static Color
sample(const uniform Map *            uniform  map,
             uniform ShadingTLState * uniform  tls,
       const varying State &                   state)
{
    Color sample = Color_ctor(
        evalAttrR(map, tls, state),
        evalAttrG(map, tls, state),
        evalAttrB(map, tls, state));
    return sample;
}

DEFINE_MAP_SHADER(FloatToRgbMap, sample)

