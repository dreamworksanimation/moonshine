// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

struct NormalToRgbMap
{
    const uniform NormalMap * uniform mNormalMap;
    uniform intptr_t mSampleNormalFunc;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(NormalToRgbMap);

static Color
sample(const uniform Map* uniform map,
       uniform ShadingTLState* uniform tls,
       const varying State& state)
{
    const uniform NormalToRgbMap * uniform me = MAP_GET_ISPC_PTR(NormalToRgbMap, map);
    varying Vec3f input = Vec3f_ctor(0.0f, 0.0f, 0.0f);
    SampleNormalPtr sampleNormalfn = (SampleNormalPtr)me->mSampleNormalFunc;
    if (sampleNormalfn != NULL) sampleNormalfn(me->mNormalMap, tls, &state, &input);
    return Color_ctor(input.x, input.y, input.z);
}

DEFINE_MAP_SHADER(NormalToRgbMap, sample)

