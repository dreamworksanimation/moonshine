// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ProjectCameraMap.ispc

#include "attributes.isph"

#include <moonray/map/primvar/ispc/Primvar.isph>
#include <moonshine/map/projection/ispc/Projection.isph>

#include <moonray/rendering/shading/ispc/MapApi.isph>

struct ProjectCameraMap
{
    const uniform Xform * uniform mXform;
    uniform int mRefPKey;
    uniform int mRefNKey;
    uniform bool mHasValidProjector;
    uniform Xform3f s2uv;

    uniform PROJECTION_StaticData * uniform mStaticData;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(ProjectCameraMap);


static Color
sample(const uniform Map *            uniform map,
             uniform ShadingTLState * uniform tls,
       const varying State &                  state)
{
    const uniform ProjectCameraMap * uniform me = MAP_GET_ISPC_PTR(ProjectCameraMap, map);

    Color result = Color_ctor(0.0f,
                              0.0f,
                             -1.0f);

    if (!me->mHasValidProjector) {
        logEvent(map, me->mStaticData->sErrorMissingProjector);
        return result;
    }

    // Get the position in screen space and normal in projection camera's space
    uniform int inputSourceMode;
    if (getAttrUseReferenceSpace(map)) {
        inputSourceMode = INPUT_SOURCE_MODE_REF_P_REF_N;
    } else {
        inputSourceMode = INPUT_SOURCE_MODE_P_N;
    }

    varying Vec3f P_s, pos_ddx, pos_ddy, pos_ddz, inputPosition;
    if (!PRIMVAR_getPosition(tls, state,
                             inputSourceMode,
                             inputPosition,
                             me->mXform,
                             SHADING_SPACE_SCREEN,
                             me->mRefPKey,
                             P_s, pos_ddx, pos_ddy, pos_ddz)) {
        // Log missing ref_P data message
        logEvent(map, me->mStaticData->sErrorMissingRefP);
        return result;
    }

    varying Vec3f N_c = Vec3f_ctor(0.f, 0.f, -1.f); // default to back facing

    if (!getAttrProjectOnBackFaces(map)) {
        varying Vec3f inputNormal;
        if (!PRIMVAR_getNormal(tls, state,
                               inputSourceMode,
                               inputNormal,
                               me->mXform,
                               SHADING_SPACE_CAMERA,
                               me->mRefPKey,
                               me->mRefNKey,
                               N_c)) {
            // Log missing ref_N data message
            logEvent(map, me->mStaticData->sErrorMissingRefN);
            return result;
        }
    }

    // Set blue/w component based on UV region and back facing.  If UVs are
    // outside the 0.0 to 1.0 range or the surface is back facing and project
    // on back faces is off, the mask is set to -1.0.   If they are inside
    // the range the mask is set to 1.0.   This value can be used by downstream
    // maps (i.e. ImageMap) to mask these regions.
    varying Vec3f uvw;
    const bool frontFacing = N_c.z > 0.f;
    if (getAttrProjectOnBackFaces(map) || frontFacing) {
        // Transform screen space position to UV space
        varying Vec3f U = transformPoint(me->s2uv, P_s);
        uvw = U;
        if (getAttrBlackOutsideProjection(map)) {
            uvw.z = (uvw.x < 0.f || uvw.x > 1.f || uvw.y < 0.f || uvw.y > 1.f) ? -1.f : 1.f;
        } else {
            // set w coord to 1 to let ImageMap know this coord is valid
            uvw.z = 1.f;
        }
        return asAColor(uvw);
    } else {
        uvw = Vec3f_ctor(0.0f, 0.0f, -1.0f);
        return asAColor(uvw);
    }
}

DEFINE_MAP_SHADER(ProjectCameraMap, sample)

