// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.isph"

#include <moonray/map/primvar/ispc/Primvar.isph>
#include <moonshine/map/projection/ispc/Projection.isph>

#include <moonray/rendering/shading/ispc/MapApi.isph>

struct ProjectCylindricalMap
{
    const uniform Xform * uniform mXform;
    uniform int mRefPKey;
    uniform int mRefNKey;
    uniform bool mHasValidProjector;

    uniform PROJECTION_StaticData * uniform mStaticData;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(ProjectCylindricalMap);

static varying Vec3f
cylinderProjection(const varying Vec3f& point)
{
    const varying float len = length(Vec2f_ctor(point.x, point.z));
    if (isZero(len)) return point;

    // Set projection cy
    Vec3f result = Vec3f_ctor(0.0f, point.y + 0.5f, 0.0f);

    // Set projection s
    const varying float theta = 0.5f * dw_acos(point.x / len) / sPi;
    result.x = point.z >= 0 ? (1.0f - theta) : theta;

    return result;
}

static Color
sample(const uniform Map* uniform map, uniform ShadingTLState* uniform tls, const varying State& state)
{
    const uniform ProjectCylindricalMap* uniform me = MAP_GET_ISPC_PTR(ProjectCylindricalMap, map);

    Color result = Color_ctor(0.0f, 0.0f, -1.0f);

    if (!me->mHasValidProjector) {
        logEvent(map, me->mStaticData->sErrorMissingProjector);
        return Color_ctor(0.0f, 0.0f, 0.0f);
    }

    // Retrieve position and normal in object space
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
        return result;
    }

    uniform SHADING_Space returnSpace;
    if (getAttrUseReferenceSpace(map)) {
        returnSpace = SHADING_SPACE_WORLD;
    } else {
        returnSpace = SHADING_SPACE_RENDER;
    }

    varying Vec3f normal, inputNormal;
    if (!PRIMVAR_getNormal(tls, state,
                           inputSourceMode,
                           inputNormal,
                           me->mXform,
                           returnSpace,
                           me->mRefPKey,
                           me->mRefNKey,
                           normal)) {
        // Log missing ref_N data message
        logEvent(map, me->mStaticData->sErrorMissingRefN);
        return result;
    }

    // check normal, possibly exit early
    if (!getAttrProjectInward(map) || !getAttrProjectOutward(map)) {
        // transform normal to object space of projector
        normal = transformNormal(me->mXform,
                                 SHADING_SPACE_RENDER,
                                 SHADING_SPACE_OBJECT,
                                 state,
                                 normal);

        // project pos vector onto plane defined by cylinder axis
        varying Vec3f pc = pos;
        pc.y = 0.0f;

        const varying Vec3f nc = isEntering(state) ? normal : neg(normal);

        const varying float pDotN = dot(pc, nc);


        if ((!getAttrProjectInward(map) && pDotN < 0.0f) ||
            (!getAttrProjectOutward(map) && pDotN > 0.0f)) {
            // return the sentinal value for invalid texture coords
            // as a signal to ImageMap not to project the texture
            return Color_ctor(0.0f, 0.0f, -1.0f);
        }
    }

    // Project position into cylindrical coords
    varying Vec3f rPos = cylinderProjection(pos);

    // Set blue/w component based on UV region.  If UVs are outside the 0.0 
    // to 1.0 range, the mask is set to 0.0.   If they are inside the range 
    // the mask is set to 1.0.
    if (getAttrBlackOutsideProjection(map)) {
        rPos.z = (rPos.x < 0.0f || rPos.x > 1.0f || rPos.y < 0.0f || rPos.y > 1.0f) ? -1.0f : 1.0f;
    }
    
    return asAColor(rPos);
}

DEFINE_MAP_SHADER(ProjectCylindricalMap, sample)

