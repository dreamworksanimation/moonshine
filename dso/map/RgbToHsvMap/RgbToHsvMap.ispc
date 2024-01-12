// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file RgbToHsvMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>
#include <scene_rdl2/common/math/ispc/ColorSpace.isph>

static Color
sample(const uniform Map* uniform map,
       uniform ShadingTLState* uniform tls,
       const varying State& state)
{
    const varying Color input = evalAttrInput(map, tls, state);
    varying Color sample = rgbToHsv(input);
    return sample;
}

DEFINE_MAP_SHADER(RgbToHsvMap, sample)

