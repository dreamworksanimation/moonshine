// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file HairColorPresetsMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

enum HairColor {
    BLACK               = 0,
    GRAY                = 1,
    PLATINUM_BLOND      = 2,
    LIGHT_BLOND         = 3,
    GOLDEN_BLOND        = 4,
    STRAWBERRY_BLOND    = 5,
    LIGHT_RED           = 6,
    DARK_RED            = 7,
    LIGHT_AUBURN        = 8,
    DARK_AUBURN         = 9,
    BROWN               = 10,
    DARK_BROWN          = 11,
    GOLDEN_BROWN        = 12,
    ASH_BROWN           = 13,
    CHESTNUT_BROWN      = 14,
};

ISPC_UTIL_EXPORT_ENUM_TO_HEADER(HairColor);

static Color
sample(const uniform Map *uniform map,
       uniform ShadingTLState * uniform tls,
       const varying State &state)
{
    Color result;

    switch(getAttrColor(map)) {
        case BLACK:
            result = Color_ctor(0.0008f, 0.0008f, 0.0007f);
            break;
        case GRAY:
            result = Color_ctor(0.1669f, 0.1562f, 0.1502f);
            break;
        case PLATINUM_BLOND:
            result = Color_ctor(0.829f, 0.640525f, 0.418645f);
            break;
        case LIGHT_BLOND:
            result = Color_ctor(0.5f, 0.392323f, 0.250527f);
            break;
        case GOLDEN_BLOND:
            result = Color_ctor(0.287f, 0.18693f, 0.09499f);
            break;
        case STRAWBERRY_BLOND:
            result = Color_ctor(0.50543f, 0.17491f, 0.07663f);
            break;
        case LIGHT_RED:
            result = Color_ctor(0.6196f, 0.10853f, 0.04877f);
            break;
        case DARK_RED:
            result = Color_ctor(0.3577f, 0.0866f, 0.0696f);
            break;
        case LIGHT_AUBURN:
            result = Color_ctor(0.3804f, 0.1175f, 0.056614f);
            break;
        case DARK_AUBURN:
            result = Color_ctor(0.1115f, 0.056614f, 0.036655f);
            break;
        case BROWN:
            result = Color_ctor(0.1449f, 0.07382f, 0.05112f);
            break;
        case DARK_BROWN:
            result = Color_ctor(0.05261f, 0.03341f, 0.01774f);
            break;
        case GOLDEN_BROWN:
            result = Color_ctor(0.1175f, 0.08513f, 0.0469f);
            break;
        case ASH_BROWN:
            result = Color_ctor(0.4159f, 0.2555f, 0.157f);
            break;
        case CHESTNUT_BROWN:
            result = Color_ctor(0.07806f, 0.05459f, 0.05459f);
            break;
        default:
            result = Color_ctor(0.0f, 0.0f, 0.0f);
            break;
    }

    return result;
}

DEFINE_MAP_SHADER(HairColorPresetsMap, sample)

