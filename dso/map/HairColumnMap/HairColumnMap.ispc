// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file HairColumnMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

struct HairColumnMap
{
    uniform int mScatterTagKey;
};

ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(HairColumnMap);

static Color
sample(const uniform Map* uniform map,
       uniform ShadingTLState* uniform tls,
       const varying State& state)
{
    Color sample;

    const uniform HairColumnMap * uniform me = MAP_GET_ISPC_PTR(HairColumnMap, map);

    float scatterTag = getFloatAttribute(tls, state, me->mScatterTagKey);
    if (isEqual(scatterTag, -1.f)) {
        sample = Color_ctor(0.f, 0.f, 0.f);
    } else {
        Vec2f st = getSt(state);
        sample = Color_ctor(scatterTag, state.mSt.x, 0.0f);
    }
    return sample;
}

DEFINE_MAP_SHADER(HairColumnMap, sample)

