// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file LODMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

enum LODMapType {
    FEATURE_WIDTH = 0,
    CAMERA_DISTANCE = 1
};

ISPC_UTIL_EXPORT_ENUM_TO_HEADER(LODMapType);

struct LODMap
{
    const uniform Xform* uniform mXform;
    uniform float mStart;
    uniform float mRange;
};

ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(LODMap);

static Color
sample(const uniform Map *            uniform  map,
             uniform ShadingTLState * uniform  tls,
       const varying State &                   state)
{
    varying Color sample = sBlack;

    const uniform LODMap * uniform me = MAP_GET_ISPC_PTR(LODMap, map);

    float featureValue = 0.0f;

    if (getAttrLODType(map) == FEATURE_WIDTH) {
        // Get Feature Width
        featureValue = (length(getdPdx(state)) + length(getdPdy(state)))*0.5f;
    } else {
        // Get Camera Distance
        Vec3f pos = state.mP;
        pos = transformPoint(me->mXform,
                             SHADING_SPACE_RENDER,
                             SHADING_SPACE_CAMERA,
                             state,
                             pos);
        featureValue = length(pos);
    }

    const float lodValue = saturate((featureValue - me->mStart) / me->mRange);

    const Color colorA = evalAttrNearValue(map, tls, state);
    const Color colorB = evalAttrFarValue(map, tls, state);

    sample = lerp(colorA, colorB, lodValue);

    return sample;
}

DEFINE_MAP_SHADER(LODMap, sample)

