// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
#include "TriplanarTexture.h"
#include "ProjectionUtil.h"
#include <scene_rdl2/render/util/Random.h>
#include <scene_rdl2/render/util/stdmemory.h>

#define BLEND_EPSILON 0.0005f

namespace moonshine {
namespace projection {

using namespace scene_rdl2::math;

TriplanarTexture::TriplanarTexture(
        scene_rdl2::rdl2::Shader *map,
        scene_rdl2::util::Random& rand,
        const ispc::TEXTURE_GammaMode gammaMode,
        const std::string& filename,
        bool active, bool wrap, bool invertS, bool invertT, bool swapST,
        float rotation,
        const Vec2f& offset,
        const Vec2f& scale,
        const Vec2f& rotationCenter,
        bool randomFlip, bool randomOffset, bool randomRotation,
        scene_rdl2::logging::LogEventRegistry& logEventRegistry,
        const Color& fatalColor) :
    mActive(active),
    mTexture(fauxstd::make_unique<moonray::shading::BasicTexture>(map, logEventRegistry)),
    mScale(scale),
    mFatalColor(fatalColor)
{

    composeTransform2D(rand,
                       offset, scale, rotation,
                       rotationCenter,
                       invertS, invertT,
                       swapST,
                       randomFlip, randomOffset, randomRotation,
                       m2DXform);

    std::string errorStr;

    moonray::shading::WrapType wrapS, wrapT;
    if (wrap) {
        wrapS = moonray::shading::WrapType::Periodic;
        wrapT = moonray::shading::WrapType::Periodic;
    } else {
        wrapS = moonray::shading::WrapType::Clamp;
        wrapT = moonray::shading::WrapType::Clamp;
    }

    mValid = mTexture->update(filename,
                              gammaMode,
                              wrapS,
                              wrapT,
                              false,            // use default color
                              sBlack,     // default color
                              mFatalColor,      // fatal color
                              errorStr);
    if (!mValid) {
        map->fatal(errorStr);
    }
}

Color
TriplanarTexture::sample(
        const Vec2f& st,
        float (&derivatives)[4],
        moonray::shading::TLState* tls,
        const moonray::shading::State& shadingState)
{
    if (!mValid) {
        return mFatalColor;
    } else if (!mActive) {
        return sBlack;
    } else {
        return mTexture->sample(tls, shadingState, st, derivatives);
    }
}

void
composeTransform2D(
    scene_rdl2::util::Random& rand,
    Vec2f offset,
    Vec2f scale,
    float rotation,
    const Vec2f& rotationCenter,
    bool invertS,
    bool invertT,
    bool swapST,
    bool randomizeFlip,
    bool randomizeOffset,
    bool randomizeRotation,
    Xform3f& xform)
{
    scale.x = 1.0f / scene_rdl2::math::max(scale.x, sEpsilon) * ((invertS) ? -1.0f : 1.0f);
    scale.y = 1.0f / scene_rdl2::math::max(scale.y, sEpsilon) * ((invertT) ? -1.0f : 1.0f);
    if (randomizeFlip) {
        scale.x *= ((rand.getNextFloat() > 0.5f) ? -1 : 1);
        scale.y *= ((rand.getNextFloat() > 0.5f) ? -1 : 1);
    }

    // Flip X so positive values move the texture to the right
    offset.x *= -1.0f;
    if (randomizeOffset) {
        offset.x += (rand.getNextFloat() - 0.5f);
        offset.y += (rand.getNextFloat() - 0.5f);
    }
    rotation = -1.0f * deg2rad(rotation) + (swapST ? 90.0f : 0.0f);
    if (randomizeRotation) {
        rotation += rand.getNextFloat() * sTwoPi - sPi;
    }

    xform = Xform3f(scene_rdl2::math::one);

    // To Rotation Center
    xform = xform * Xform3f::translate(
        Vec3f(-rotationCenter.x, -rotationCenter.y, 0.0f));

    // Rotation
    xform = xform * Xform3f::rotate(
        Vec3f(0.0f, 0.0f, 1.0f), rotation);

    // From Rotation Center
    xform = xform * Xform3f::translate(
        Vec3f(rotationCenter.x, rotationCenter.y, 0.0f));

    // Scale
    xform = xform * Xform3f::scale(
        Vec3f(scale.x, scale.y, 0.0f));

    // Translate
    xform = xform * Xform3f::translate(
        Vec3f(offset.x, offset.y, 0.0f));
}

float
calculateTransitionWidthPower(float transitionWidth)
{
    // Pre-condition "transition width" parameter, which acts as the
    // exponent for the normal blending, for an intuitive feel from 0..1

    float twp = 1.0f - clamp(transitionWidth, 0.0f, 1.0f);

    // raise to ^5 for a smoother feel at 0.5
    twp = twp * twp * twp * twp * twp;
    // Limit the max value to avoid too much transition
    twp = 0.025f + (twp * 0.975f);
    // Multiply by 100.0 for sharp transition at 0.0
    twp *= 100.0f;

    return twp;
}

Vec3f
calculateTriplanarBlending(const Vec3f& normal,
                           float twp)
{
    Vec3f result = scene_rdl2::math::abs(normal);
    result.x = scene_rdl2::math::pow(result.x, twp);
    result.y = scene_rdl2::math::pow(result.y, twp);
    result.z = scene_rdl2::math::pow(result.z, twp);
    result /= (result.x + result.y + result.z);
    return result;
}

void
fillTriplanarTextureIndices(
    const int numTextures,
    int (&indexArray)[6])
{
    if (numTextures == ispc::PROJECTION_NUM_TEXTURES_ONE) {
        indexArray[0] = 0;
        indexArray[1] = 0;
        indexArray[2] = 0;
        indexArray[3] = 0;
        indexArray[4] = 0;
        indexArray[5] = 0;
    } else if (numTextures == ispc::PROJECTION_NUM_TEXTURES_THREE) {
        indexArray[0] = 0;
        indexArray[1] = 1;
        indexArray[2] = 2;
        indexArray[3] = 0;
        indexArray[4] = 1;
        indexArray[5] = 2;
    } else if (numTextures == ispc::PROJECTION_NUM_TEXTURES_SIX) {
        indexArray[0] = 0;
        indexArray[1] = 1;
        indexArray[2] = 2;
        indexArray[3] = 3;
        indexArray[4] = 4;
        indexArray[5] = 5;
    }
}

void
swizzleTriplanarFace(const Vec3f &P,
                     const Vec3f &dPdx,
                     const Vec3f &dPdy,
                     const Vec3f &dPdz,
                     const int cubeface,
                     Vec3f &result,
                     Vec3f &result_ddx,
                     Vec3f &result_ddy,
                     Vec3f &result_ddz)
{
    MNRY_ASSERT(cubeface >= 0 && cubeface < 6);

    // We assume that the 'P' and its partials have already been transformed
    // into the object space of the triplanar projector.

    // The y coordinate is negated since OIIO uses (0,0) as upper left
    // st coordinate, however the partial derivs are not. I am not exactly
    // sure why this needs to be this way, but it is based on rendering
    // a set of six planes (with standard tex coords) arranged to form
    // a cube (identical in arrangement and orientation to our triplanar
    // projection) and inspecting the resulting partial derivatives.
    // The partial derivatives computed below exactly match those of the
    // six arranged planes.
    switch(cubeface) {
    //                              new X     new Y     new Z
    case 0:
        // facing positive x
        result      = Vec3f( -P.z,     -P.y,     -P.x);
        result_ddx  = Vec3f( -dPdx.z,   dPdx.y,  -dPdx.x);
        result_ddy  = Vec3f( -dPdy.z,   dPdy.y,  -dPdy.x);
        result_ddz  = Vec3f( -dPdz.z,   dPdz.y,  -dPdz.x);
        break;
    case 1:
        // facing positive y
        result      = Vec3f( P.x,       P.z,     -P.y);
        result_ddx  = Vec3f( dPdx.x,   -dPdx.z,  -dPdx.y);
        result_ddy  = Vec3f( dPdy.x,   -dPdy.z,  -dPdy.y);
        result_ddz  = Vec3f( dPdz.x,   -dPdz.z,  -dPdz.y);
        break;
    case 2:
        // facing positive z
        result      = Vec3f( P.x,      -P.y,     -P.z);
        result_ddx  = Vec3f( dPdx.x,    dPdx.y,  -dPdx.z);
        result_ddy  = Vec3f( dPdy.x,    dPdy.y,  -dPdy.z);
        result_ddz  = Vec3f( dPdz.x,    dPdz.y,  -dPdz.z);
        break;
    case 3:
        // facing negative x
        result      = Vec3f( P.z,      -P.y,      P.x);
        result_ddx  = Vec3f( dPdx.z,    dPdx.y,   dPdx.x);
        result_ddy  = Vec3f( dPdy.z,    dPdy.y,   dPdy.x);
        result_ddz  = Vec3f( dPdz.z,    dPdz.y,   dPdz.x);
        break;
    case 4:
        // facing negative y
        result      = Vec3f( P.x,      -P.z,      P.y);
        result_ddx  = Vec3f( dPdx.x,    dPdx.z,   dPdx.y);
        result_ddy  = Vec3f( dPdy.x,    dPdy.z,   dPdy.y);
        result_ddz  = Vec3f( dPdz.x,    dPdz.z,   dPdz.y);
        break;
    case 5:
        // facing negative z
        result      = Vec3f( -P.x,     -P.y,      P.z);
        result_ddx  = Vec3f( -dPdx.x,   dPdx.y,   dPdx.z);
        result_ddy  = Vec3f( -dPdy.x,   dPdy.y,   dPdy.z);
        result_ddz  = Vec3f( -dPdz.x,   dPdz.y,   dPdz.z);
        break;
    }
}

void
updateTriplanarMap(scene_rdl2::rdl2::Shader* map,
                   const ispc::TEXTURE_GammaMode gammaMode,
                   const int randomSeed,
                   const bool randomizeFlip,
                   const bool randomizeOffset,
                   const bool randomizeRotation,
                   const float transitionWidth,
                   const int numTextures,
                   const scene_rdl2::rdl2::SceneObject* projectorObject,
                   const ispc::PROJECTION_Mode projectionMode,
                   const Mat4d &projectionMatrix,
                   const ispc::PROJECTION_TransformOrder transformOrder,
                   const ispc::PROJECTION_RotationOrder rotationOrder,
                   const Vec3f& translation,
                   const Vec3f& rotation,
                   const Vec3f& scale,
                   const TriplanarFaceAttrs& faceAttrs,
                   const Color& fatalColor,
                   scene_rdl2::logging::LogEventRegistry& logEventRegistry,
                   ispc::PROJECTION_TriplanarData& outputIspcData,
                   std::array<std::unique_ptr<projection::TriplanarTexture>, 6>& outputTriplanarTextures,
                   std::unique_ptr<moonray::shading::Xform>& outputProjectorXform)
{
    scene_rdl2::util::Random rand(randomSeed);

    // Construct texture objects for each face of the projection
    for (int i = 0; i < numTextures; ++i) {
        outputTriplanarTextures[i] =
            fauxstd::make_unique<projection::TriplanarTexture>(map,
                                                               rand,
                                                               gammaMode,
                                                               map->get(faceAttrs.mTextureAttrs[i]),
                                                               map->get(faceAttrs.mActiveAttrs[i]),
                                                               map->get(faceAttrs.mWrapAttrs[i]),
                                                               map->get(faceAttrs.mInvSAttrs[i]),
                                                               map->get(faceAttrs.mInvTAttrs[i]),
                                                               map->get(faceAttrs.mSwapSTAttrs[i]),
                                                               map->get(faceAttrs.mRotAttrs[i]),
                                                               map->get(faceAttrs.mOffsetAttrs[i]),
                                                               map->get(faceAttrs.mScaleAttrs[i]),
                                                               map->get(faceAttrs.mRotCntrAttrs[i]),
                                                               randomizeFlip,
                                                               randomizeOffset,
                                                               randomizeRotation,
                                                               logEventRegistry, 
                                                               fatalColor);

        outputIspcData.mTriplanarTextures[i].mTexture = &outputTriplanarTextures[i]->mTexture->getBasicTextureData();
        outputIspcData.mTriplanarTextures[i].mActive = outputTriplanarTextures[i]->mActive;
        outputIspcData.mTriplanarTextures[i].mValid = outputTriplanarTextures[i]->mValid;
        asCpp(outputIspcData.mTriplanarTextures[i].mScale) = outputTriplanarTextures[i]->mScale;
        asCpp(outputIspcData.mTriplanarTextures[i].m2DXform) = outputTriplanarTextures[i]->m2DXform;
    }

    // Set texture indices based on "number of textures" parameter
    projection::fillTriplanarTextureIndices(numTextures, outputIspcData.mTextureIndices);

    // Pre-condition "transition width" parameter, which acts as the
    // exponent for the normal blending, for an intuitive feel from 0..1
    outputIspcData.mTransitionWidthPower = 
        projection::calculateTransitionWidthPower(transitionWidth);

    outputIspcData.mHasValidProjector = false;
    outputIspcData.mProjectorXform = nullptr;
    outputProjectorXform = getProjectorXform(map,
                                             projectionMode,
                                             projectorObject,
                                             projectionMatrix,
                                             transformOrder,
                                             rotationOrder,
                                             translation,
                                             rotation,
                                             scale);
    if (outputProjectorXform != nullptr) {
        outputIspcData.mProjectorXform = outputProjectorXform->getIspcXform();
        outputIspcData.mHasValidProjector = true;
    }
}

void
fillTriplanarTextures(moonray::shading::TLState* tls,
                      const moonray::shading::State& state, 
                      const int* textureIndices,
                      const std::array<std::unique_ptr<TriplanarTexture>, 6>& outputTriplanarTextures,
                      const float transitionWidthPower,
                      const Vec3f& normal,
                      const Vec3f& pos,
                      const Vec3f& pos_ddx,
                      const Vec3f& pos_ddy,
                      const Vec3f& pos_ddz,
                      Vec3f& outputBlending,
                      Color* outputColors)
{
    outputBlending = calculateTriplanarBlending(normal,
                                                transitionWidthPower);

    // As an optimization, if the outputBlending value for an axis is less
    // than BLEND_EPSILON we skip it to avoid the expensive texture
    // lookup.  The value was chosen to minimize the difference with
    // the non-optimized version.   Values at or lower than 0.0005f
    // show no difference with r_diff.
    for (size_t cubeface = 0; cubeface < 6; ++cubeface) {
        outputColors[cubeface] = Color(0.f, 0.f, 0.f);

        if  (((cubeface == 0 || cubeface == 3) && (outputBlending.x < BLEND_EPSILON)) ||
             ((cubeface == 1 || cubeface == 4) && (outputBlending.y < BLEND_EPSILON)) ||
             ((cubeface == 2 || cubeface == 5) && (outputBlending.z < BLEND_EPSILON))) continue;

        // Get our position w/ partial derivs as projected onto this cube face
        Vec3f U, dUdx, dUdy, dUdz;
        projection::swizzleTriplanarFace(pos, pos_ddx, pos_ddy, pos_ddz,
                                         cubeface,
                                         U, dUdx, dUdy, dUdz);

        // apply our 2D texture transform, while the center is (0, 0)
        const int index = textureIndices[cubeface];
        const Xform3f &xform = outputTriplanarTextures[index]->m2DXform;
        U = transformPoint(xform, U);
        dUdx = transformVector(xform, dUdx);
        dUdy = transformVector(xform, dUdy);

        // offset point so center is (0.5, 0.5)
        const Vec2f st(U.x + 0.5f, U.y + 0.5f);

        float derivatives[4];
        derivatives[0] = dUdx.x; // dsdx
        derivatives[1] = dUdx.y; // dtdx
        derivatives[2] = dUdy.x; // dsdy
        derivatives[3] = dUdy.y; // dtdy

        // Sample the texture
        outputColors[cubeface] = outputTriplanarTextures[index]->sample(st, derivatives, tls, state);
    }
}

void
fillTriplanarNormalTextures(const int normalMapEncoding,
                            moonray::shading::TLState* tls,
                            const moonray::shading::State& state, 
                            const int* textureIndices,
                            const std::array<std::unique_ptr<TriplanarTexture>, 6>& outputTriplanarTextures,
                            const float transitionWidthPower,
                            const Vec3f& normal,
                            const Vec3f& pos,
                            const Vec3f& pos_ddx,
                            const Vec3f& pos_ddy,
                            const Vec3f& pos_ddz,
                            Vec3f& outputBlending,
                            Vec3f* outputNormals,
                            float* outputNormalLengths)
{
    outputBlending = calculateTriplanarBlending(normal,
                                                transitionWidthPower);

    // As an optimization, if the outputBlending value for an axis is less
    // than BLEND_EPSILON we skip it to avoid the expensive texture
    // lookup.  The value was chosen to minimize the difference with
    // the non-optimized version.   Values at or lower than 0.0005f
    // show no difference with r_diff.
    for (size_t cubeface = 0; cubeface < 6; ++cubeface) {
        outputNormals[cubeface] = normal;
        if  (((cubeface == 0 || cubeface == 3) && (outputBlending.x < BLEND_EPSILON)) ||
             ((cubeface == 1 || cubeface == 4) && (outputBlending.y < BLEND_EPSILON)) ||
             ((cubeface == 2 || cubeface == 5) && (outputBlending.z < BLEND_EPSILON))) {
            // Initialize value to state normal length so we don't get NaNs.
            outputNormalLengths[cubeface] = 1.f;
            continue;
        }

        // Get 'U', our position w/ partial derivs as projected onto this cube face
        Vec3f U, dUdx, dUdy, dUdz;
        projection::swizzleTriplanarFace(pos, pos_ddx, pos_ddy, pos_ddz,
                                         cubeface,
                                         U, dUdx, dUdy, dUdz);

        // apply our 2D texture transform, while the center is (0, 0)
        const int index = textureIndices[cubeface];
        const Xform3f &xform = outputTriplanarTextures[index]->m2DXform;
        U = transformPoint(xform, U);
        dUdx = transformVector(xform, dUdx);
        dUdy = transformVector(xform, dUdy);

        // offset point so center is (0.5, 0.5)
        const Vec2f st(U.x + 0.5f, U.y + 0.5f);

        float derivatives[4];
        derivatives[0] = dUdx.x; // dsdx
        derivatives[1] = dUdx.y; // dtdx
        derivatives[2] = dUdy.x; // dsdy
        derivatives[3] = dUdy.y; // dtdy

        // Sample the texture
        Color tx = outputTriplanarTextures[index]->sample(st, derivatives, tls, state);

        Vec3f tNormal =  Vec3f(tx.r, tx.g, tx.b);
        outputNormalLengths[cubeface] = length(tNormal);

        // Remap the normals from the texture texture map (which, by default,
        // are assumed to be in the 0 to 1 range) to the -1 to 1 range.
        if (normalMapEncoding == 0) { // [0,1]
            tNormal = 2.0f * Vec3f(tx.r, tx.g, tx.b) - Vec3f(1.0f);
            outputNormalLengths[cubeface] = length(tNormal);
            tNormal = tNormal / outputNormalLengths[cubeface]; // normalize
        }

        // projector object-space tangent
        Vec3f tangent;
        switch (cubeface) {
        case 0:
            tangent = Vec3f( 0.f,  0.f, -1.f);
            break;
        case 1:
            tangent = Vec3f( 1.f,  0.f,  0.f);
            break;
        case 2:
            tangent = Vec3f( 1.f,  0.f,  0.f);
            break;
        case 3:
            tangent = Vec3f( 0.f,  0.f,  1.f);
            break;
        case 4:
            tangent = Vec3f( 1.f,  0.f,  0.f);
            break;
        case 5:
            tangent = Vec3f(-1.f,  0.f,  0.f);
            break;
        }

        bool reversedNormals = false;
        if (state.isProvided(moonray::shading::StandardAttributes::sReversedNormals)) {
            reversedNormals = state.getAttribute(moonray::shading::StandardAttributes::sReversedNormals);
        }
        if (reversedNormals) {
            tangent *= -1.0f;
        }

        // projector object-space ReferenceFrame
        ReferenceFrame frame(normal, tangent);

        outputNormals[cubeface] = frame.localToGlobal(tNormal);
    }
}


} // projection
} // moonshine


