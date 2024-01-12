// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.isph"

#include <moonray/map/primvar/ispc/Primvar.isph>
#include <moonshine/map/projection/ispc/Projection.isph>
#include <moonshine/map/projection/ispc/TriplanarTexture.isph>

#include <moonray/rendering/shading/ispc/BasicTexture.isph>
#include <moonray/rendering/shading/ispc/MapApi.isph>

struct ProjectTriplanarNormalMap
{
    uniform PROJECTION_TriplanarData mTriplanarData;
    uniform PROJECTION_StaticData * uniform mStaticData;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(ProjectTriplanarNormalMap);

static Vec3f
sampleNormal(const uniform NormalMap* uniform map,
             uniform ShadingTLState* uniform tls,
             const varying State& state) {

    const uniform ProjectTriplanarNormalMap * uniform me = MAP_GET_ISPC_CPTR(ProjectTriplanarNormalMap, map);
    const uniform PROJECTION_TriplanarData& data = me->mTriplanarData;

    if (!data.mHasValidProjector) {
        logEvent(map, me->mStaticData->sErrorMissingProjector);
        return state.mN;
    }

    // Retrieve position and normal and transform into
    // the object space of the projector
    uniform int inputSourceMode;
    if (getAttrUseReferenceSpace(map)) {
        inputSourceMode = INPUT_SOURCE_MODE_REF_P_REF_N;
    } else {
        inputSourceMode = INPUT_SOURCE_MODE_P_N;
    }

    Vec3f pos, pos_ddx, pos_ddy, pos_ddz, inputPosition;
    if (!PRIMVAR_getPosition(tls, state,
                             inputSourceMode,
                             inputPosition,
                             data.mProjectorXform,
                             SHADING_SPACE_OBJECT,
                             data.mRefPKey,
                             pos, pos_ddx, pos_ddy, pos_ddz)) {
        // Log missing ref_P data message
        logEvent(map, me->mStaticData->sErrorMissingRefP);
        return state.mN;
    }

    varying Vec3f normal, inputNormal;
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
        return state.mN;
    }

    const Vec3f nml = normal;

    Vec3f blending;
    Vec3f oNormal[6];
    float tNormalLength[6];
    PROJECTION_fillTriplanarNormalTextures(getAttrNormalEncoding(map),
                                           tls, state,
                                           data.mTextureIndices,
                                           data.mTriplanarTextures,
                                           data.mTransitionWidthPower,
                                           data.mReversedNormalsIndx,
                                           nml, 
                                           pos, pos_ddx, pos_ddy, pos_ddz,
                                           blending,
                                           oNormal,
                                           tNormalLength);

    // Blend directions
    const Vec3f bNormal = ((nml.x > 0.0f) ? oNormal[0] : oNormal[3]) * blending.x +
                          ((nml.y > 0.0f) ? oNormal[1] : oNormal[4]) * blending.y +
                          ((nml.z > 0.0f) ? oNormal[2] : oNormal[5]) * blending.z;

    const float bNormalLength =
        ((nml.x > 0.0f) ? tNormalLength[0] : tNormalLength[3]) * blending.x +
        ((nml.y > 0.0f) ? tNormalLength[1] : tNormalLength[4]) * blending.y +
        ((nml.z > 0.0f) ? tNormalLength[2] : tNormalLength[5]) * blending.z;

    varying Vec3f rNormal;
    if (getAttrUseReferenceSpace(map)) {
        // Transform blended normal from the object space of the
        // projector to world space
        const varying Vec3f pNormal = transformNormal(data.mProjectorXform,
            SHADING_SPACE_OBJECT, SHADING_SPACE_WORLD,
            state, bNormal);

        // Transform blended normal from world space to the
        // space of the rendered object and then to render space
        rNormal = transformNormal(data.mObjXform,
            SHADING_SPACE_OBJECT, SHADING_SPACE_RENDER,
            state, pNormal);
    } else {
        // Transform blended normal from the object space of the
        // projector to world space
        rNormal = transformNormal(data.mProjectorXform,
            SHADING_SPACE_OBJECT, SHADING_SPACE_RENDER,
            state, bNormal);
    }

    // bNormalLength is used to preserve the original length of the normal,
    // which is necessary for the Toksvig mapping technique which recalculates
    // the roughness value based on the length of the input normal in the Dwa materials.
    // adjust the normal as necessary with to avoid black
    // artifacts at oblique viewing angles.
    rNormal = bNormalLength * normalize(rNormal);
    return rNormal;
}

DEFINE_NORMALMAP_SHADER(ProjectTriplanarNormalMap, sampleNormal)

