// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.isph"

#include <moonray/map/primvar/ispc/Primvar.isph>
#include <moonshine/map/projection/ispc/Projection.isph>
#include <moonshine/map/projection/ispc/TriplanarTexture.isph>

#include <moonray/rendering/shading/ispc/BasicTexture.isph>
#include <moonray/rendering/shading/ispc/Shading.isph>
#include <scene_rdl2/common/math/ispc/asA.isph>
#include <scene_rdl2/common/math/ispc/Xform.isph>
#include <scene_rdl2/common/platform/IspcUtil.isph>

struct ProjectTriplanarNormalMap_v2
{
    uniform PROJECTION_TriplanarData mTriplanarData;
    uniform PROJECTION_StaticData * uniform mStaticData;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(ProjectTriplanarNormalMap_v2);

static Vec3f
sampleNormal(const uniform NormalMap* uniform map,
             uniform ShadingTLState* uniform tls,
             const varying State& state) {

    const uniform ProjectTriplanarNormalMap_v2 * uniform me =
        MAP_GET_ISPC_CPTR(ProjectTriplanarNormalMap_v2, map);

    const uniform PROJECTION_TriplanarData& data = me->mTriplanarData;

    if (!data.mHasValidProjector) {
        logEvent(map, tls, me->mStaticData->sErrorMissingProjector);
        return state.mN;
    }

    // Retrieve position and normal and transform into
    // the object space of the projector
    uniform int inputSourceMode = getAttrInputSource(map);

    Vec3f pos, pos_ddx, pos_ddy, pos_ddz, inputPosition;
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
        logEvent(map, tls, me->mStaticData->sErrorMissingRefP);
        return state.mN;
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
        logEvent(map, tls, me->mStaticData->sErrorMissingRefN);
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
    if(inputSourceMode == INPUT_SOURCE_MODE_REF_P_REF_N) {
        // Transform blended normal from the object space of the
        // projector to world space
        const varying Vec3f pNormal = transformNormal(data.mProjectorXform,
            SHADING_SPACE_OBJECT, SHADING_SPACE_WORLD,
            state, bNormal);

        Vec3f refN;
        if (!SHADER_DATA_getRefN(data.mRefPKey, data.mRefNKey, tls, state, &refN)) {
            logEvent(map, tls, me->mStaticData->sErrorMissingRefN);
        }
        refN = normalize(refN);

        Vec3f dPds;
        if (isProvided(state, data.mdPdsKey)) {
            dPds = normalize(getVec3fAttribute(tls, state, data.mdPdsKey));
        } else if (isdsProvided(state, data.mRefPKey)) {
            dPds = normalize(getdVec3fAttributeds(tls, state, data.mRefPKey));
        } else {
            logEvent(map, tls, me->mStaticData->sErrorMissingdPds);
        }

        // Transform from tangent space to world space 
        ReferenceFrame refFrame;
        ReferenceFrame_init(refFrame, refN, dPds);
        Vec3f tNormal = globalToLocal(refFrame, pNormal);

        // Transform from world space to render space
        ReferenceFrame curFrame;
        ReferenceFrame_init(curFrame, getN(state), normalize(getdPds(state)));
        rNormal = localToGlobal(curFrame, tNormal);
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

DEFINE_NORMALMAP_SHADER(ProjectTriplanarNormalMap_v2, sampleNormal)

