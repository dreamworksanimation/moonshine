// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.isph"

#include <moonray/map/primvar/ispc/Primvar.isph>
#include <moonshine/map/projection/ispc/Projection.isph>
#include <moonshine/map/projection/ispc/TriplanarTexture.isph>

#include <moonray/rendering/shading/ispc/MapApi.isph>

struct ProjectPlanarNormalMap
{
    const uniform Xform * uniform mProjectorXform;
    const uniform Xform * uniform mObjXform;
    uniform int mRefPKey;
    uniform int mRefNKey;
    uniform int mdPdsKey;
    uniform bool mHasValidProjector;
    uniform float mTransitionWidthPower;
    uniform Color mFatalColor;
    const uniform BASIC_TEXTURE_Data * uniform mTexture;
    uniform PROJECTION_StaticData * uniform mStaticData;
    uniform int mReversedNormalsIndx;
};

ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(ProjectPlanarNormalMap);

bool
isOutside(const varying Vec2f& uv) {
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f) {
        return true;
    } else {
        return false;
    }
}

static Vec3f
sampleNormal(const uniform NormalMap* uniform map,
             uniform ShadingTLState* uniform tls,
             const varying State& state) {

    const uniform ProjectPlanarNormalMap* uniform me = NORMALMAP_GET_ISPC_PTR(ProjectPlanarNormalMap, map);

    if (!me->mHasValidProjector) {
        logEvent(map, tls, me->mStaticData->sErrorMissingProjector);
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

    varying Vec3f U, dUdx, dUdy, dUdz, inputPosition;
    if (!PRIMVAR_getPosition(tls, state,
                             inputSourceMode,
                             inputPosition,
                             me->mProjectorXform,
                             SHADING_SPACE_OBJECT,
                             me->mRefPKey,
                             U, dUdx, dUdy, dUdz)) {
        // Log missing ref_P data message
        logEvent(map, tls, me->mStaticData->sErrorMissingRefP);
        return state.mN;
    }

    varying Vec3f normal, inputNormal;
    if (!PRIMVAR_getNormal(tls, state,
                           inputSourceMode,
                           inputNormal,
                           me->mProjectorXform,
                           SHADING_SPACE_OBJECT,
                           me->mRefPKey,
                           me->mRefNKey,
                           normal)) {
        // Log missing ref_N data message
        logEvent(map, tls, me->mStaticData->sErrorMissingRefN);
        return state.mN;
    }

    const Vec3f nml = normal;

    // Get 'U', our position w/ partial derivs as projected onto the front
    // cube face which is set as 2 below.
    PROJECTION_swizzleTriplanarFace(U, dUdx, dUdy, dUdz, 2,
                                    &U, &dUdx, &dUdy, &dUdz);
    const varying Vec3f uvw = U;

    // offset point so center is (0.5, 0.5)
    Vec2f st = Vec2f_ctor(uvw.x + 0.5f, uvw.y + 0.5f);

    // If the "wrap around" parameter is false and we are outside the 
    // 0 to 1 range, then just return the shading normal.
    if (!getAttrWrapAround(map) && isOutside(st)) {
        const Vec3f& N = getN(state);
        return N;
    }

    // Sample the tangent space normal texture usingthe swizzled derivatives
    float derivatives[4];
    derivatives[0] = dUdx.x; // dsdx
    derivatives[1] = dUdx.y; // dtdx
    derivatives[2] = dUdy.x; // dsdy
    derivatives[3] = dUdy.y; // dtdy
    Col4f tx = BASIC_TEXTURE_sample(me->mTexture, tls, state, st, derivatives);

    Vec3f tNormal = Vec3f_ctor(tx.r, tx.g, tx.b);
    float tNormalLength = length(tNormal);

    // Remap the normals from the texture texture map (which 
    // are assumed to be in the 0 to 1 range) to the -1 to 1 range.
    if (getAttrNormalEncoding(map) == 0) { // [0,1]
        tNormal = 2.0f * tNormal - Vec3f_ctor(1.0f, 1.0f, 1.0f);
        tNormalLength = length(tNormal);
        tNormal = tNormal / tNormalLength; // normalize
    }

    // We flip the x component of the normal on three of the
    // sides to match the look of it's opposite side
    Vec3f tNormalFlippedX = Vec3f_ctor(-tNormal.x, tNormal.y, tNormal.z);

    // Blend the six object space normals based on the input // normal direction
    Vec3f blending = PROJECTION_calculateTriplanarBlending(nml,
                                                           me->mTransitionWidthPower);

    Vec3f oNormal[6];
    for (uniform int cubeface = 0; cubeface < 6; ++cubeface) {
        oNormal[cubeface] = nml;

        // projector object-space tangent
        varying Vec3f tangent;
        switch (cubeface) {
        case 0:
            tangent = Vec3f_ctor( 0.f,  0.f, -1.f);
            break;
        case 1:
            tangent = Vec3f_ctor( 1.f,  0.f,  0.f);
            break;
        case 2:
            tangent = Vec3f_ctor( 1.f,  0.f,  0.f);
            break;
        case 3:
            tangent = Vec3f_ctor( 0.f,  0.f,  1.f);
            break;
        case 4:
            tangent = Vec3f_ctor( 1.f,  0.f,  0.f);
            break;
        case 5:
            tangent = Vec3f_ctor(-1.f,  0.f,  0.f);
            break;
        }

        varying bool reversedNormals = false;
        if (isProvided(state, me->mReversedNormalsIndx)) {
            reversedNormals = getBoolAttribute(tls, state, me->mReversedNormalsIndx);
        }
        if (reversedNormals) {
            tangent = tangent * -1.0f;
        }

        if (isZero(cross(nml, tangent))) continue;

        // Create reference frame for each direction to transform
        // TBN normal into projector space
        const ReferenceFrame frame = ReferenceFrame_ctor(nml, tangent);

        // Use x flipped tNormal for +x, -y, and -z faces
        if (cubeface == 0 || cubeface == 4 || cubeface == 5) {
            oNormal[cubeface] = localToGlobal(frame, tNormalFlippedX);
        } else {
            oNormal[cubeface] = localToGlobal(frame, tNormal);
        }
    }

    // Blend directions
    const Vec3f bNormal = ((nml.x > 0.0f) ? oNormal[0] : oNormal[3]) * blending.x +
                          ((nml.y > 0.0f) ? oNormal[1] : oNormal[4]) * blending.y +
                          ((nml.z > 0.0f) ? oNormal[2] : oNormal[5]) * blending.z;

    varying Vec3f rNormal;
    if (getAttrUseReferenceSpace(map)) {
        // Transform blended normal from the object space of the
        // projector to world space
        const varying Vec3f pNormal = transformNormal(me->mProjectorXform,
            SHADING_SPACE_OBJECT, SHADING_SPACE_WORLD,
            state, bNormal);

        Vec3f refN;
        if (!SHADER_DATA_getRefN(me->mRefPKey, me->mRefNKey, tls, state, &refN)) {
            logEvent(map, tls, me->mStaticData->sErrorMissingRefN);
        }
        refN = normalize(refN);

        Vec3f dPds;
        if (isProvided(state, me->mdPdsKey)) {
            // Explicitly authored "dPds" primitive attribute
            dPds = normalize(getVec3fAttribute(tls, state, me->mdPdsKey));
        } else if (isdsProvided(state, me->mRefPKey)) {
            dPds = normalize(getdVec3fAttributeds(tls, state, me->mRefPKey));
        } else {
            logEvent(map, tls, me->mStaticData->sErrorMissingdPds);
        }

        // Transform from tangent space to world space 
        ReferenceFrame refFrame;
        ReferenceFrame_init(refFrame, refN, dPds);
        Vec3f tNorm = globalToLocal(refFrame, pNormal);

        // Transform from world space to render space
        ReferenceFrame curFrame;
        ReferenceFrame_init(curFrame, getN(state), normalize(getdPds(state)));
        rNormal = localToGlobal(curFrame, tNorm);
    } else {
        // Transform blended normal from the object space of the
        // projector to world space
        rNormal = transformNormal(me->mProjectorXform,
            SHADING_SPACE_OBJECT, SHADING_SPACE_RENDER,
            state, bNormal);
    }

    // bNormalLength is used to preserve the original length of the normal,
    // which is necessary for the Toksvig mapping technique which recalculates
    // the roughness value based on the length of the input normal in the Dwa materials.
    // adjust the normal as necessary with to avoid black
    // artifacts at oblique viewing angles.
    const float bNormalLength = tNormalLength * (blending.x + blending.y + blending.z);
    rNormal = bNormalLength * normalize(rNormal);

    return rNormal;
}

DEFINE_NORMALMAP_SHADER(ProjectPlanarNormalMap, sampleNormal)

