// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file Projection.ispc

#include "TriplanarTexture.isph"

#define BLEND_EPSILON 0.0005f

ISPC_UTIL_EXPORT_ENUM_TO_HEADER(PROJECTION_NumTextures);
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(PROJECTION_TriplanarData);

void
PROJECTION_swizzleTriplanarFace(
    const varying Vec3f& pos,
    const varying Vec3f& dPdx,
    const varying Vec3f& dPdy,
    const varying Vec3f& dPdz,
    const varying int axis,
    varying Vec3f * result,
    varying Vec3f * result_ddx,
    varying Vec3f * result_ddy,
    varying Vec3f * result_ddz)
{
    MNRY_ASSERT(axis >= 0 && axis < 6);

    // We assume that the 'pos' has already been transformed
    // into the object space of the triplanar projector.

    // The y coordinate is negated since OIIO uses (0,0) as upper left
    // st coordinate, however the partial derivs are not. I am not exactly
    // sure why this needs to be this way, but it is based on rendering
    // a set of six planes (with standard tex coords) arranged to form
    // a cube (identical in arrangement and orientation to our triplanar
    // projection) and inspecting the resulting partial derivatives.
    // The partial derivatives computed below exactly match those of the
    // six arranged planes.
    switch(axis) {
    //                              new X     new Y     new Z
    case 0:
        // facing positive x
        *result      = Vec3f_ctor( -pos.z,    -pos.y,   -pos.x);
        *result_ddx  = Vec3f_ctor( -dPdx.z,   dPdx.y,  -dPdx.x);
        *result_ddy  = Vec3f_ctor( -dPdy.z,   dPdy.y,  -dPdy.x);
        *result_ddz  = Vec3f_ctor( -dPdz.z,   dPdz.y,  -dPdz.x);
        break;
    case 1:
        // facing positive y
        *result      = Vec3f_ctor( pos.x,      pos.z,   -pos.y);
        *result_ddx  = Vec3f_ctor( dPdx.x,   -dPdx.z,  -dPdx.y);
        *result_ddy  = Vec3f_ctor( dPdy.x,   -dPdy.z,  -dPdy.y);
        *result_ddz  = Vec3f_ctor( dPdz.x,   -dPdz.z,  -dPdz.y);
        break;
    case 2:
        // facing positive z
        *result      = Vec3f_ctor( pos.x,     -pos.y,   -pos.z);
        *result_ddx  = Vec3f_ctor( dPdx.x,    dPdx.y,  -dPdx.z);
        *result_ddy  = Vec3f_ctor( dPdy.x,    dPdy.y,  -dPdy.z);
        *result_ddz  = Vec3f_ctor( dPdz.x,    dPdz.y,  -dPdz.z);
        break;
    case 3:
        // facing negative x
        *result      = Vec3f_ctor( pos.z,     -pos.y,    pos.x);
        *result_ddx  = Vec3f_ctor( dPdx.z,    dPdx.y,   dPdx.x);
        *result_ddy  = Vec3f_ctor( dPdy.z,    dPdy.y,   dPdy.x);
        *result_ddz  = Vec3f_ctor( dPdz.z,    dPdz.y,   dPdz.x);
        break;
    case 4:
        // facing negative y
        *result      = Vec3f_ctor( pos.x,     -pos.z,    pos.y);
        *result_ddx  = Vec3f_ctor( dPdx.x,    dPdx.z,   dPdx.y);
        *result_ddy  = Vec3f_ctor( dPdy.x,    dPdy.z,   dPdy.y);
        *result_ddz  = Vec3f_ctor( dPdz.x,    dPdz.z,   dPdz.y);
        break;
    case 5:
        // facing negative z
        *result      = Vec3f_ctor( -pos.x,    -pos.y,    pos.z);
        *result_ddx  = Vec3f_ctor( -dPdx.x,   dPdx.y,   dPdx.z);
        *result_ddy  = Vec3f_ctor( -dPdy.x,   dPdy.y,   dPdy.z);
        *result_ddz  = Vec3f_ctor( -dPdz.x,   dPdz.y,   dPdz.z);
        break;
    }    
}

Col4f
PROJECTION_sampleTriplanarTexture(
    const uniform PROJECTION_TriplanarTexture &texture, 
    const varying Vec2f& st, 
    varying float (&derivatives)[4],
    uniform ShadingTLState* uniform tls,
    const varying State& state)
{
    if (!texture.mValid) {
        return Col4f_ctor(texture.mFatalColor.r,
                          texture.mFatalColor.g,
                          texture.mFatalColor.b,
                          1.f);
    } else if (!texture.mActive) {
        return Col4f_ctor(0.0f);
    } else {
        return BASIC_TEXTURE_sample(texture.mTexture, tls, state, st, derivatives);
    }
}

Vec3f
PROJECTION_calculateTriplanarBlending(const Vec3f& normal, float twp) {
    Vec3f result = abs(normal);
    result.x = pow(result.x, twp);
    result.y = pow(result.y, twp);
    result.z = pow(result.z, twp);
    result = result /(result.x + result.y + result.z);
    return result;
}

void
PROJECTION_fillTriplanarTextures(uniform ShadingTLState* uniform tls,
                                 const varying State& state,
                                 const uniform int * uniform textureIndices,
                                 const uniform PROJECTION_TriplanarTexture * uniform triplanarTextures,
                                 const uniform float transitionWidthPower,
                                 const varying Vec3f& normal,
                                 const varying Vec3f& pos,
                                 const varying Vec3f& pos_ddx,
                                 const varying Vec3f& pos_ddy,
                                 const varying Vec3f& pos_ddz,
                                 varying Vec3f& outputBlending,
                                 varying Col4f * outputColors)
{
    outputBlending = PROJECTION_calculateTriplanarBlending(normal,
                                                           transitionWidthPower);

    // As an optimization, if the blending value for an axis is less
    // than BLEND_EPSILON we skip it to avoid the expensive texture
    // lookup.  The value was chosen to minimize the difference with
    // the non-optimized version.   Values at or lower than 0.0005f
    // show no difference with r_diff.
    for (uniform int cubeface = 0; cubeface < 6; ++cubeface) {
        const uniform int index = textureIndices[cubeface];

        outputColors[cubeface] = Col4f_ctor(0.f, 0.f, 0.f, 0.f);

        if  (((cubeface == 0 || cubeface == 3) && (outputBlending.x < BLEND_EPSILON)) ||
             ((cubeface == 1 || cubeface == 4) && (outputBlending.y < BLEND_EPSILON)) ||
             ((cubeface == 2 || cubeface == 5) && (outputBlending.z < BLEND_EPSILON))) continue;

        // Get our position w/ partial derivs as projected onto this cube face
        varying Vec3f U, dUdx, dUdy, dUdz;
        PROJECTION_swizzleTriplanarFace(pos, pos_ddx, pos_ddy, pos_ddz,
                                        cubeface,
                                        &U, &dUdx, &dUdy, &dUdz);

        // apply our 2D texture transform, while the center is (0, 0)
        U = transformPoint(triplanarTextures[index].m2DXform, U);
        dUdx = transformVector(triplanarTextures[index].m2DXform, dUdx);
        dUdy = transformVector(triplanarTextures[index].m2DXform, dUdy);

        // offset point so center is (0.5, 0.5)
        U = U + Vec3f_ctor(0.5f, 0.5f, 0.f);

        const Vec2f st = Vec2f_ctor(U.x, U.y);

        float derivatives[4];
        derivatives[0] = dUdx.x; // dsdx
        derivatives[1] = dUdx.y; // dtdx
        derivatives[2] = dUdy.x; // dsdy
        derivatives[3] = dUdy.y; // dtdy

        // Sample the texture
        outputColors[cubeface] = PROJECTION_sampleTriplanarTexture(triplanarTextures[index],
                                                                   st, derivatives, tls, state);
    }
}

void
PROJECTION_fillTriplanarNormalTextures(const int normalMapEncoding,
                                       uniform ShadingTLState* uniform tls,
                                       const varying State& state,
                                       const uniform int * uniform textureIndices,
                                       const uniform PROJECTION_TriplanarTexture * uniform triplanarTextures,
                                       const uniform float transitionWidthPower,
                                       const uniform int reversedNormalsIndx,
                                       const varying Vec3f& normal,
                                       const varying Vec3f& pos,
                                       const varying Vec3f& pos_ddx,
                                       const varying Vec3f& pos_ddy,
                                       const varying Vec3f& pos_ddz,
                                       varying Vec3f& outputBlending,
                                       varying Vec3f * outputNormals,
                                       varying float * outputNormalLengths)
{
    outputBlending = PROJECTION_calculateTriplanarBlending(normal,
                                                           transitionWidthPower);

    // As an optimization, if the blending value for an axis is less
    // than BLEND_EPSILON we skip it to avoid the expensive texture
    // lookup.  The value was chosen to minimize the difference with
    // the non-optimized version.   Values at or lower than 0.0005f
    // show no difference with r_diff.
    for (uniform int cubeface = 0; cubeface < 6; ++cubeface) {
        const uniform int index = textureIndices[cubeface];

        outputNormals[cubeface] = normal;

        if  (((cubeface == 0 || cubeface == 3) && (outputBlending.x < BLEND_EPSILON)) ||
             ((cubeface == 1 || cubeface == 4) && (outputBlending.y < BLEND_EPSILON)) ||
             ((cubeface == 2 || cubeface == 5) && (outputBlending.z < BLEND_EPSILON))) {
            // Initialize value to state normal length so we don't get NaNs.
            outputNormalLengths[cubeface] = 1.f;
            continue;
        }

        // Get our position w/ partial derivs as projected onto this cube face
        varying Vec3f U, dUdx, dUdy, dUdz;
        PROJECTION_swizzleTriplanarFace(pos, pos_ddx, pos_ddy, pos_ddz,
                                        cubeface,
                                        &U, &dUdx, &dUdy, &dUdz);

        // apply our 2D texture transform, while the center is (0, 0)
        U = transformPoint(triplanarTextures[index].m2DXform, U);
        dUdx = transformVector(triplanarTextures[index].m2DXform, dUdx);
        dUdy = transformVector(triplanarTextures[index].m2DXform, dUdy);

        // offset point so center is (0.5, 0.5)
        Vec2f st = Vec2f_ctor(U.x + 0.5f, U.y + 0.5f);

        float derivatives[4];
        derivatives[0] = dUdx.x; // dsdx
        derivatives[1] = dUdx.y; // dtdx
        derivatives[2] = dUdy.x; // dsdy
        derivatives[3] = dUdy.y; // dtdy

        // Sample the texture
        Col4f tx = PROJECTION_sampleTriplanarTexture(triplanarTextures[index],
                                                     st, derivatives, tls, state);

        Vec3f tNormal = Vec3f_ctor(tx.r, tx.g, tx.b);
        outputNormalLengths[cubeface] = length(tNormal);

        // Remap the normals from the texture texture map (which
        // are assumed, by default, to be in the 0 to 1 range) to the -1 to 1 range.
        if (normalMapEncoding == 0) { // [0,1]
            tNormal = 2.0f * tNormal - Vec3f_ctor(1.0f, 1.0f, 1.0f);
            outputNormalLengths[cubeface] = length(tNormal);
            tNormal = tNormal / outputNormalLengths[cubeface]; // normalize
        }

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
        if (isProvided(state, reversedNormalsIndx)) {
            reversedNormals = getBoolAttribute(tls, state, reversedNormalsIndx);
        }
        if (reversedNormals) {
            tangent = tangent * -1.0f;
        }

        // Create reference frame for each direction to transform
        // TBN normal into projector space
        ReferenceFrame frame;
        ReferenceFrame_init(frame, normal, tangent);

        outputNormals[cubeface] = localToGlobal(frame, tNormal);
    }
}

