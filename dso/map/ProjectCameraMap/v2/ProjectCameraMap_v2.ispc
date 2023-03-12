// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ProjectCameraMap_v2.ispc

#include "attributes.isph"

#include <moonray/map/primvar/ispc/Primvar.isph>
#include <moonshine/map/projection/ispc/Projection.isph>

#include <moonray/rendering/shading/ispc/BasicTexture.isph>
#include <moonray/rendering/shading/ispc/MapApi.isph>

struct ProjectCameraMap_v2
{
    const uniform Xform * uniform mXform;
    uniform int mRefPKey;
    uniform int mRefNKey;
    uniform bool mHasValidProjector;
    uniform Xform3f s2uv;
    uniform Color mFatalColor;
    const uniform BASIC_TEXTURE_Data * uniform mTexture;

    uniform PROJECTION_StaticData * uniform mStaticData;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(ProjectCameraMap_v2);

enum AspectRatioSource {
    ASPECT_SOURCE_TEXTURE = 0,
    ASPECT_SOURCE_CUSTOM  = 1,
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(AspectRatioSource);

static Color
sample(const uniform Map *            uniform map,
             uniform ShadingTLState * uniform tls,
       const varying State &                  state)
{
    const uniform ProjectCameraMap_v2 * uniform me = MAP_GET_ISPC_PTR(ProjectCameraMap_v2, map);

    Color result = Color_ctor(0.f, 0.f, 0.f);

    if (!me->mHasValidProjector) {
        // Log missing projector data message
        logEvent(map, tls, me->mStaticData->sErrorMissingProjector);
        return result;
    }

    // Retrieve position with partial derivs in screen space.
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
        logEvent(map, tls, me->mStaticData->sErrorMissingRefP);
        return me->mStaticData->sFatalColor;
    }

    // check to see if the point is within the frustum
    if (getAttrBlackOutsideProjection(map) &&
        (P_s.x < -1.f || P_s.x > 1.f || P_s.y < -1.f || P_s.y > 1.f)) {
        return result;
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
            logEvent(map, tls, me->mStaticData->sErrorMissingRefN);
            return result;
        }

    }
    const varying bool isFrontFacing = Ncam.z > 0.0f;

    if (getAttrProjectOnBackFaces(map) || isFrontFacing) {
        // Transform from screen space to UV space (which maps from [-1,1] to [0,1] on xy),
        // 'U' is our UV coordinate
        const varying Vec3f U = transformPoint(me->s2uv, P_s);
        const varying Vec3f dUdx = transformVector(me->s2uv, pos_ddx);
        const varying Vec3f dUdy = transformVector(me->s2uv, pos_ddy);

        varying float derivatives[4];
        derivatives[0] = dUdx.x;    // dsdx
        derivatives[1] = dUdx.y;    // dtdx
        derivatives[2] = dUdy.x;    // dsdy
        derivatives[3] = dUdy.y;    // dtdy

        const varying Vec3f uvw = U;
        const varying Vec2f st = Vec2f_ctor(uvw.x, uvw.y);

        // sample the texture
        {
            varying const Col4f tx = BASIC_TEXTURE_sample(me->mTexture, tls, state, st, derivatives);
            if (getAttrAlphaOnly(map)) {
                result = Color_ctor(tx.a, tx.a, tx.a);
            } else {
                result = Color_ctor(tx.r, tx.g, tx.b);
                if (getAttrUnpremultiply(map) && (tx.a > 0.001f) && (tx.a < 1.0f)) {
                    const varying float alpha = tx.a;
                    result.r = result.r / alpha;
                    result.g = result.g / alpha;
                    result.b = result.b / alpha;
                }
            }
        }
    }

    return result;
}

DEFINE_MAP_SHADER(ProjectCameraMap_v2, sample)

