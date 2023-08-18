// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.isph"

#include <moonray/map/primvar/ispc/Primvar.isph>
#include <moonshine/map/projection/ispc/Projection.isph>
#include <moonshine/map/projection/ispc/TriplanarTexture.isph>

#include <moonray/rendering/shading/ispc/BasicTexture.isph>
#include <moonray/rendering/shading/ispc/MapApi.isph>

struct ProjectTriplanarMap
{
    uniform PROJECTION_TriplanarData mTriplanarData;
    uniform PROJECTION_StaticData * uniform mStaticData;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(ProjectTriplanarMap);

static Color
sample(const uniform Map* uniform map, uniform ShadingTLState* uniform tls, const varying State& state) {
    const uniform ProjectTriplanarMap * uniform me = MAP_GET_ISPC_CPTR(ProjectTriplanarMap, map);
    const uniform PROJECTION_TriplanarData data = me->mTriplanarData;

    if (!data.mHasValidProjector) {
        logEvent(map, me->mStaticData->sErrorMissingProjector);
        return me->mStaticData->sFatalColor;
    }

    // Retrieve position and normal and transform into
    // the object space of the projector
    uniform int inputSourceMode = getAttrInputSource(map);

    varying Vec3f pos, pos_ddx, pos_ddy, pos_ddz, inputPosition;
    if (inputSourceMode == INPUT_SOURCE_MODE_ATTR) {
        inputPosition = evalAttrInputPosition(map, tls, state);
    }
    if (!PRIMVAR_getPosition(tls, state,
                             inputSourceMode,
                             inputPosition,
                             data.mProjectorXform,
                             SHADING_SPACE_OBJECT,
                             data.mRefPKey,
                             pos, pos_ddx, pos_ddy, pos_ddz)) {
        // Log missing ref_P data message
        logEvent(map, me->mStaticData->sErrorMissingRefP);
        return me->mStaticData->sFatalColor;
    }

    varying Vec3f normal, inputNormal;
    if (inputSourceMode == INPUT_SOURCE_MODE_ATTR) {
        inputNormal = evalAttrInputNormal(map, tls, state);
    }
    if (!PRIMVAR_getNormal(tls, state,
                           inputSourceMode,
                           inputNormal,
                           data.mProjectorXform,
                           SHADING_SPACE_OBJECT,
                           data.mRefPKey,
                           data.mRefNKey,
                           normal)) {
        // Log missing ref_N data message
        logEvent(map, me->mStaticData->sErrorMissingRefN);
        return me->mStaticData->sFatalColor;
    }

    Vec3f blending;
    Col4f tx[6];
    PROJECTION_fillTriplanarTextures(tls, state,
                                     data.mTextureIndices,
                                     data.mTriplanarTextures,
                                     data.mTransitionWidthPower,
                                     normal, pos, pos_ddx, pos_ddy, pos_ddz,
                                     blending, tx);

    // Blend directions
    const Col4f result =
        ((normal.x > 0.0f) ? tx[0] : tx[3]) * blending.x +
        ((normal.y > 0.0f) ? tx[1] : tx[4]) * blending.y +
        ((normal.z > 0.0f) ? tx[2] : tx[5]) * blending.z;

    return Color_ctor(result.r, result.g, result.b);
}

DEFINE_MAP_SHADER(ProjectTriplanarMap_v2, sample)

