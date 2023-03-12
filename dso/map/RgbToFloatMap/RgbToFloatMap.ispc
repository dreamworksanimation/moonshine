// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file RgbToFloatMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

enum RgbToFloatMode {
    R = 0,
    G = 1,
    B = 2,
    MIN = 3,
    MAX = 4,
    AVERAGE = 5,
    SUM = 6,
    LUMINANCE = 7
};

ISPC_UTIL_EXPORT_ENUM_TO_HEADER(RgbToFloatMode);

struct RgbToFloatMap
{
    uniform int mMode;
};

ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(RgbToFloatMap);

static Color
sample(const uniform Map *            uniform  map,
             uniform ShadingTLState * uniform  tls,
       const varying State &                   state)
{
    const uniform RgbToFloatMap * uniform rgb2floatmap =
        MAP_GET_ISPC_CPTR(RgbToFloatMap, map);
    Color input = evalAttrInput(map, tls, state);
    Color sample;
    switch (rgb2floatmap->mMode) {
    case R:
        sample = Color_ctor(input.r);
        break;
    case G:
        sample = Color_ctor(input.g);
        break;
    case B:
        sample = Color_ctor(input.b);
        break;
    case MIN:
        sample = Color_ctor(min(min(input.r, input.g), input.b));
        break;
    case MAX:
        sample = Color_ctor(max(max(input.r, input.g), input.b));
        break;
    case AVERAGE:
        sample = Color_ctor((input.r + input.g + input.b) / 3.0f);
        break;
    case SUM:
        sample = Color_ctor(input.r + input.g + input.b);
        break;
    case LUMINANCE:
        sample = Color_ctor(luminance(input));
        break;
    default:
        break;
    }
    return sample;
}

DEFINE_MAP_SHADER(RgbToFloatMap, sample)

