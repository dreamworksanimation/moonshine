// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.isph"

#include <moonray/map/primvar/ispc/Primvar.isph>
#include <moonshine/map/projection/ispc/Projection.isph>

#include <moonray/rendering/shading/ispc/MapApi.isph>

struct ProjectPlanarMap
{
    const uniform Xform * uniform mXform;
    uniform int mRefPKey;
    uniform int mRefNKey;
    uniform bool mHasValidProjector;
    uniform Xform3f o2uv;

    uniform PROJECTION_StaticData * uniform mStaticData;
};

ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(ProjectPlanarMap);

static Color
sample(const uniform Map* uniform map, uniform ShadingTLState* uniform tls, const varying State& state)
{
    const uniform ProjectPlanarMap* uniform me = MAP_GET_ISPC_PTR(ProjectPlanarMap, map);

    Color result = Color_ctor(0.0f, 0.0f, -1.0f);

    if (!me->mHasValidProjector) {
        logEvent(map, me->mStaticData->sErrorMissingProjector);
        return result;
    }

    // Retrieve position and normal in object space
    uniform int inputSourceMode;
    if (getAttrUseReferenceSpace(map)) {
        inputSourceMode = INPUT_SOURCE_MODE_REF_P_REF_N;
    } else {
        inputSourceMode = INPUT_SOURCE_MODE_P_N;
    }

    varying Vec3f P_o, pos_ddx, pos_ddy, pos_ddz, inputPosition;
    if (!PRIMVAR_getPosition(tls, state,
                             inputSourceMode,
                             inputPosition,
                             me->mXform,
                             SHADING_SPACE_OBJECT,
                             me->mRefPKey,
                             P_o, pos_ddx, pos_ddy, pos_ddz)) {
        // Log missing ref_P data message
        logEvent(map, me->mStaticData->sErrorMissingRefP);
        return result;
    }

    // don't test the normal unless we need to
    varying bool isBackfacing = false;
    if (!getAttrProjectOnBackFaces(map)) {
        varying Vec3f projectedN, inputNormal;
        if (!PRIMVAR_getNormal(tls, state,
                               inputSourceMode,
                               inputNormal,
                               me->mXform,
                               SHADING_SPACE_OBJECT,
                               me->mRefPKey,
                               me->mRefNKey,
                               projectedN)) {
            // Log missing ref_N data message
            logEvent(map, me->mStaticData->sErrorMissingRefN);
            return result;
        }

        if (projectedN.z < 0.f) { isBackfacing = true; }
    }

    // Set blue/w component based on UV region and back facing.  If UVs are
    // outside the 0.0 to 1.0 range or the surface is back facing and project
    // on back faces is off, the mask is set to -1.0.   If they are inside
    // the range the mask is set to 1.0.   This value can be used by downstream
    // maps (i.e. ImageMap) to mask these regions.
    varying Vec3f uvw;
    if (isBackfacing && !getAttrProjectOnBackFaces(map)) {
        // invalid tex coord
        uvw = Vec3f_ctor(0.0f, 0.0f, -1.0f);
        return asAColor(uvw);
    } else {
        // Transform screen space position to UV space
        varying Vec3f U = transformPoint(me->o2uv, P_o);
        uvw = U;
        if (getAttrBlackOutsideProjection(map)) {
            // valid tex coord ?
            uvw.z = (uvw.x < 0.f ||
                     uvw.x > 1.f ||
                     uvw.y < 0.f ||
                     uvw.y > 1.f) ? -1.f : 1.f;
        } else {
            // valid tex coord
            uvw.z = 1.f;
        }
        return asAColor(uvw);
    }
}

DEFINE_MAP_SHADER(ProjectPlanarMap, sample)

