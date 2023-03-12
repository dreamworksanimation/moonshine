// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file RandomMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

enum RANDOM_Constants {
    RANDOM_TABSIZE = 2048,
    RANDOM_INTMAX = 2147483647 // C++ INT_MAX
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(RANDOM_Constants);

struct StaticRandomMapData {
    uniform Color mRandomTable[RANDOM_TABSIZE];
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(StaticRandomMapData);

struct RandomMap
{
    uniform StaticRandomMapData * uniform mStaticData;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(RandomMap);

int getIndex(const varying float input, 
             const uniform int tabSize, 
             const uniform int seed, 
             const uniform int tabMask)
{
    // check for int overflow
    if ((int) ceil(input) < (RANDOM_INTMAX - seed) / tabSize) {
        return ((int)(input * tabSize) + seed) % tabMask;
    } else {
        const float iPart = floor(input);
        const float fPart = input - iPart;
        // expand ((i + f) * b + seed) % m with distributive to prevent overflow
        // assumed m = b - 1 ==> b % m = 1, see TABMASK = TABSIZE - 1 in sample()
        const int highP = (int)(iPart) % tabMask;
        return ((highP + (int)(fPart * tabSize) % tabMask) % tabMask + seed) % tabMask;
    }
}

static Color
sample(const uniform Map *            uniform map,
             uniform ShadingTLState * uniform tls,
       const varying State &                  state)
{
    const uniform RandomMap * uniform me = MAP_GET_ISPC_CPTR(RandomMap, map);

    const varying Color input      = evalAttrInput(map, tls, state);
    const uniform int   seed       = getAttrSeed(map);
    const uniform float outMin     = getAttrOutMin(map);
    const uniform float outMax     = getAttrOutMax(map);
    const uniform bool  monochrome = getAttrMonochrome(map);

    const uniform int TABMASK = RANDOM_TABSIZE - 1;

    Color result;
    if (monochrome) {
        int idxR = getIndex(input.r, RANDOM_TABSIZE, seed, TABMASK);
        result = Color_ctor(me->mStaticData->mRandomTable[idxR].r,
                            me->mStaticData->mRandomTable[idxR].r,
                            me->mStaticData->mRandomTable[idxR].r);
    } else {
        int idxR = getIndex(input.r, RANDOM_TABSIZE, seed + 0, TABMASK);
        int idxG = getIndex(input.g, RANDOM_TABSIZE, seed + 1, TABMASK);
        int idxB = getIndex(input.b, RANDOM_TABSIZE, seed + 2, TABMASK);
        result = Color_ctor(me->mStaticData->mRandomTable[idxR].r,
                            me->mStaticData->mRandomTable[idxG].g,
                            me->mStaticData->mRandomTable[idxB].b);
    }

    result = result * (outMax - outMin) + Color_ctor(outMin);

    return result;
}
DEFINE_MAP_SHADER(RandomMap, sample)

