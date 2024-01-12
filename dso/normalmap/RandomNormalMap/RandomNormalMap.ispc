// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file RandomNormalMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

enum RANDOM_Constants {
    RANDOM_TABSIZE = 2048,
    RANDOM_INTMAX = 2147483647 // C++ INT_MAX
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(RANDOM_Constants);

struct StaticRandomNormalMapData {
    uniform float mRandomTable[RANDOM_TABSIZE];
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(StaticRandomNormalMapData);

struct RandomNormalMap
{
    uniform StaticRandomNormalMapData * uniform mStaticData;
    uniform int mReversedNormalsIndx;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(RandomNormalMap);

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

static Vec3f
sampleNormal(const uniform NormalMap* uniform map,
             uniform ShadingTLState* uniform tls,
             const varying State& state)
{
    const uniform RandomNormalMap * uniform me = MAP_GET_ISPC_CPTR(RandomNormalMap, map);

    const varying Color input = evalAttrInput(map, tls, state);
    const uniform int seed = getAttrSeed(map);
    const uniform int TABMASK = RANDOM_TABSIZE - 1;

    const int idx1 = getIndex(input.r,        RANDOM_TABSIZE, seed, TABMASK);
    const int idx2 = getIndex(input.r * 2.5f, RANDOM_TABSIZE, seed, TABMASK); //arbitrary mult for hashing
    const float u1 = me->mStaticData->mRandomTable[idx1];
    const float u2 = me->mStaticData->mRandomTable[idx2];

    const float phi = sTwoPi * u2;
    float cp = 0.f;
    float sp = 0.f;
    sincos(phi, &sp, &cp);

    // somewhat arbitrary disk sampling, with z
    // biased to center, avoids horizon which often clips
    Vec3f normal = Vec3f_ctor(cp * u1, sp * u1, 1.f);
    normal = normalize(normal);

    varying bool reversedNormals = false;
    if (isProvided(state, me->mReversedNormalsIndx)) {
        reversedNormals = getBoolAttribute(tls, state, me->mReversedNormalsIndx);
    }
    const Vec3f statedPds = reversedNormals ? state.mdPds * -1.0f : state.mdPds * 1.0f;
    const ReferenceFrame frame = ReferenceFrame_ctor(getN(state), normalize(statedPds));
    const Vec3f renderN = localToGlobal(frame, normal);

    return renderN;
}
DEFINE_NORMALMAP_SHADER(RandomNormalMap, sampleNormal)

