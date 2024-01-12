// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ProjectCameraNormalMap.ispc

#include "attributes.isph"

#include <moonray/map/primvar/ispc/Primvar.isph>
#include <moonshine/map/projection/ispc/Projection.isph>

#include <moonray/rendering/shading/ispc/BasicTexture.isph>
#include <moonray/rendering/shading/ispc/MapApi.isph>

struct ProjectCameraNormalMap
{
    const uniform Xform * uniform mXform;
    uniform int mRefPKey;
    uniform int mRefNKey;
    uniform bool mHasValidProjector;
    uniform Xform3f s2uv;
    uniform int mReversedNormalsIndx;
    const uniform BASIC_TEXTURE_Data * uniform mTexture;

    uniform PROJECTION_StaticData * uniform mStaticData;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(ProjectCameraNormalMap);

enum AspectRatioSource {
    ASPECT_SOURCE_TEXTURE = 0,
    ASPECT_SOURCE_CUSTOM  = 1,
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(AspectRatioSource);

static Vec3f
sampleNormal(const uniform NormalMap * uniform map,
             uniform ShadingTLState * uniform tls,
             const varying State & state)
{
    const uniform ProjectCameraNormalMap * uniform me = MAP_GET_ISPC_PTR(ProjectCameraNormalMap, map);

    if (!me->mHasValidProjector) {
        // Log missing projector data message
        logEvent(map, me->mStaticData->sErrorMissingProjector);
        return state.mN;
    }

    // Retrieve position with partial derivs in screen space.
    uniform int inputSourceMode;
    if (getAttrUseReferenceSpace(map)) {
        inputSourceMode = INPUT_SOURCE_MODE_REF_P_REF_N;
    } else {
        inputSourceMode = INPUT_SOURCE_MODE_P_N;
    }

    varying Vec3f P_s, dPdx_s, dPdy_s, dPdz_s, inputPosition;
    if (!PRIMVAR_getPosition(tls, state,
                             inputSourceMode,
                             inputPosition,
                             me->mXform,
                             SHADING_SPACE_SCREEN,
                             me->mRefPKey,
                             P_s, dPdx_s, dPdy_s, dPdz_s)) {
        // Log missing ref_P data message
        logEvent(map, me->mStaticData->sErrorMissingRefP);
        return state.mN;
    }

    // check to see if the point is within the frustum
    if (P_s.x < -1.f || P_s.x > 1.f || P_s.y < -1.f || P_s.y > 1.f) {
        return state.mN;
    }

    // Retrieve normal in projection camera's space
    // only check the normal if we need to, the default means everything
    // will get projected on
    varying Vec3f Ncam = Vec3f_ctor(0.0f, 0.0f, -1.0f);
    if (!getAttrProjectOnBackFaces(map)) {
        varying Vec3f inputNormal;
        if (!PRIMVAR_getNormal(tls, state,
                               inputSourceMode,
                               inputNormal,
                               me->mXform,
                               SHADING_SPACE_CAMERA,
                               me->mRefPKey,
                               me->mRefNKey,
                               Ncam)) {
            // Log missing ref_N data message
            logEvent(map, me->mStaticData->sErrorMissingRefN);
            return state.mN;
        }
    }
    const varying bool isFrontFacing = Ncam.z > 0.0f;

    varying Vec3f normal = state.mN;
    if (getAttrProjectOnBackFaces(map) || isFrontFacing) {
        // Transform from screen space to UV space (which maps from [-1,1] to [0,1] on xy),
        // 'U' is our UV coordinate
        const varying Vec3f U = transformPoint(me->s2uv, P_s);
        const varying Vec3f dUdx = transformVector(me->s2uv, dPdx_s);
        const varying Vec3f dUdy = transformVector(me->s2uv, dPdy_s);

        varying float derivatives[4];
        derivatives[0] = dUdx.x; // dsdx
        derivatives[1] = dUdx.y; // dtdx
        derivatives[2] = dUdy.x; // dsdy
        derivatives[3] = dUdy.y; // dtdy

        const varying Vec2f st = Vec2f_ctor(U.x, U.y);

        // sample the texture
        varying Col4f tx = BASIC_TEXTURE_sample(me->mTexture, tls, state, st, derivatives);
        normal = Vec3f_ctor(tx.r, tx.g, tx.b);
    }

    // Validate normal
    if (isnan(normal.x) ||
        isnan(normal.y) ||
        isnan(normal.z) ||
        !isfinite(normal.x) ||
        !isfinite(normal.y) ||
        !isfinite(normal.z) ||
        isZero(normal)) {

        return state.mN;
    }

    // Re-center sampled normal from [0, 1] --> [-1, 1]
    // z is usually encoded from [0.5, 1.0] so that this re-centering
    // keeps it positive (in [0, 1])
    const int normalMapEncoding = getAttrNormalEncoding(map);
    if (normalMapEncoding == 0) { // [0,1]
        normal = 2.0f * normal - Vec3f_ctor(1.0f, 1.0f, 1.0f);
    }

    // Transform from tangent space to render space
    varying bool reversedNormals = false;
    if (isProvided(state, me->mReversedNormalsIndx)) {
        reversedNormals = getBoolAttribute(tls, state, me->mReversedNormalsIndx);
    }
    const Vec3f statedPds = reversedNormals ? state.mdPds * -1.0f : state.mdPds * 1.0f;
    const ReferenceFrame frame = ReferenceFrame_ctor(getN(state), normalize(statedPds));
    const Vec3f renderN = localToGlobal(frame, normal);

    // Don't normalize vector, need unnormalized length for
    // Toksvig normal anti-aliasing technique in Dwa materials.
    return renderN;
}

DEFINE_NORMALMAP_SHADER(ProjectCameraNormalMap, sampleNormal)

