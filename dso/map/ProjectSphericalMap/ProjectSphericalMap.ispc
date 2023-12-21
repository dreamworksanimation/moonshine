// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.isph"

#include <moonray/map/primvar/ispc/Primvar.isph>
#include <moonshine/map/projection/ispc/Projection.isph>

#include <moonray/rendering/shading/ispc/MapApi.isph>

struct ProjectSphericalMap
{
    const uniform Xform * uniform mXform;
    uniform bool mHasValidProjector;
    uniform int mRefPKey;

    uniform PROJECTION_StaticData * uniform mStaticData;
};

ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(ProjectSphericalMap);

static varying Vec3f
sphereProjection(const varying Vec3f& point)
{
    // Spherical projection - convert cartesian coordinates to polar coords
    varying Vec3f result = Vec3f_ctor(0.0f);

    // Set projection t
    varying float len = length(point);
    if (!isZero(len)) {
        result.y = dw_acos(point.y / len) / sPi;
    }

    // Set projection s
    len = length(Vec2f_ctor(point.x, point.z));
    if (!isZero(len)) {
        const varying float theta = 0.5f * dw_acos(point.x / len) / sPi;
        result.x = point.z >= 0 ? (1.0f - theta) : theta;
    }

    return result;
}

static Color
sample(const uniform Map* uniform map, uniform ShadingTLState* uniform tls, const varying State& state)
{
    const uniform ProjectSphericalMap* uniform me = MAP_GET_ISPC_PTR(ProjectSphericalMap, map);

    if (!me->mHasValidProjector) {
        logEvent(map, me->mStaticData->sErrorMissingProjector);
        return Color_ctor(0.0f, 0.0f, -1.0f);
    }

    // Retrieve position in object space
    uniform int inputSourceMode;
    if (getAttrUseReferenceSpace(map)) {
        inputSourceMode = INPUT_SOURCE_MODE_REF_P_REF_N;
    } else {
        inputSourceMode = INPUT_SOURCE_MODE_P_N;
    }

    varying Vec3f pos, pos_ddx, pos_ddy, pos_ddz, inputPosition;
    if (!PRIMVAR_getPosition(tls, state,
                             inputSourceMode,
                             inputPosition,
                             me->mXform,
                             SHADING_SPACE_OBJECT,
                             me->mRefPKey,
                             pos, pos_ddx, pos_ddy, pos_ddz)) {
        // Log missing ref_P data message
        logEvent(map, me->mStaticData->sErrorMissingRefP);
        return Color_ctor(0.0f, 0.0f, -1.0f);
    }

    // Project edges into polar coords
    const varying Vec3f U = sphereProjection(pos);

    return asAColor(U);
}

DEFINE_MAP_SHADER(ProjectSphericalMap, sample)

