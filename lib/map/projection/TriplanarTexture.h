// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#pragma once

#include <moonray/rendering/shading/BasicTexture.h>
#include <scene_rdl2/scene/rdl2/Map.h>
#include <scene_rdl2/scene/rdl2/AttributeKey.h>
#include <scene_rdl2/scene/rdl2/Types.h>
#include <scene_rdl2/common/math/Vec2.h>
#include <scene_rdl2/common/math/Color.h>
#include <scene_rdl2/common/math/Xform.h>
#include <scene_rdl2/render/util/Random.h>

#include "Projection_ispc_stubs.h"
#include "TriplanarTexture_ispc_stubs.h"

// Forward declarations
namespace moonray {
namespace shading { class TLState; }
}

namespace moonshine  {
namespace projection  {

class TriplanarTexture
{
public:
    TriplanarTexture(
        scene_rdl2::rdl2::Shader *map,
        scene_rdl2::util::Random& rand,
        const ispc::TEXTURE_GammaMode gammaMode,
        const std::string& filename,
        bool active,
        bool wrap,
        bool invertS,
        bool invertT,
        bool swapST,
        float rotation,
        const scene_rdl2::math::Vec2f& offset,
        const scene_rdl2::math::Vec2f& scale,
        const scene_rdl2::math::Vec2f& rotationCenter,
        bool randomFlip,
        bool randomOffset,
        bool randomRot,
        scene_rdl2::rdl2::ShaderLogEventRegistry& logEventRegistry,
        const scene_rdl2::math::Color& fatalColor
    );

    scene_rdl2::math::Color
    sample(
        const scene_rdl2::math::Vec2f& st,
        float (&derivatives)[4],
        moonray::shading::TLState* tls,
        const moonray::shading::State& shadingState
    );

    bool mActive, mValid;
    std::unique_ptr<moonray::shading::BasicTexture> mTexture;
    scene_rdl2::math::Xform3f m2DXform;
    scene_rdl2::math::Vec2f mScale;
    scene_rdl2::math::Color mFatalColor;
};

// All six faces of a triplanar map have the same set of attributes
// Collect common per-face parameters into arrays for effecient processing
struct TriplanarFaceAttrs {
    std::array<scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::String>, 6> mTextureAttrs;
    std::array<scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>, 6> mActiveAttrs;
    std::array<scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>, 6> mWrapAttrs;
    std::array<scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>, 6> mInvSAttrs;
    std::array<scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>, 6> mInvTAttrs;
    std::array<scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>, 6> mSwapSTAttrs;
    std::array<scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>, 6> mRotAttrs;
    std::array<scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Vec2f>, 6> mRotCntrAttrs;
    std::array<scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Vec2f>, 6> mOffsetAttrs;
    std::array<scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Vec2f>, 6> mScaleAttrs;
};

void
composeTransform2D(
    scene_rdl2::util::Random& rand,
    scene_rdl2::math::Vec2f offset,
    scene_rdl2::math::Vec2f scale,
    float rotation,
    const scene_rdl2::math::Vec2f& rotationCenter,
    bool invertS,
    bool invertT,
    bool swapST,
    bool randomFlip,
    bool randomOffset,
    bool randomRotation,
    scene_rdl2::math::Xform3f& xform
);

// Pre-condition "transition width" parameter, which acts as the
// exponent for the normal blending, for an intuitive feel from 0..1
float calculateTransitionWidthPower(float transitionWidth);

// Calculate blending factor for 3 directions.   The returned
// components add up to 1.0 which normalizes the blended normal.
scene_rdl2::math::Vec3f calculateTriplanarBlending(const scene_rdl2::math::Vec3f& normal,
                                              float twp);

void fillTriplanarTextureIndices(const int numTextures, int (&indexArray)[6]);

// Inputs:
// P, dPdx, dPdy, dPdz (projector's object-space)
// cubeface
//      0 : +x
//      1 : +y
//      2 : +z
//      3 : -x
//      4 : -y
//      5 : -z
//
//  Outputs:
// result, result_ddx, result_ddy, result_ddz (position w/ derivs in cube-face[i] projection-space)
void swizzleTriplanarFace(const scene_rdl2::math::Vec3f &P,
                          const scene_rdl2::math::Vec3f &dPdx,
                          const scene_rdl2::math::Vec3f &dPdy,
                          const scene_rdl2::math::Vec3f &dPdz,
                          const int cubeface,
                          scene_rdl2::math::Vec3f &result,
                          scene_rdl2::math::Vec3f &result_ddx,
                          scene_rdl2::math::Vec3f &result_ddy,
                          scene_rdl2::math::Vec3f &result_ddz);

#define ASSIGN_TRIPLANAR_FACE_ATTRS(faceAttrs) \
    faceAttrs.mTextureAttrs = {attrPXTexture, attrPYTexture, attrPZTexture, attrNXTexture, attrNYTexture, attrNZTexture}; \
    faceAttrs.mActiveAttrs = {attrPXActive, attrPYActive, attrPZActive, attrNXActive, attrNYActive, attrNZActive};        \
    faceAttrs.mWrapAttrs = {attrPXWrap, attrPYWrap, attrPZWrap, attrNXWrap, attrNYWrap, attrNZWrap};                      \
    faceAttrs.mInvSAttrs = {attrPXInvS, attrPYInvS, attrPZInvS, attrNXInvS, attrNYInvS, attrNZInvS};                      \
    faceAttrs.mInvTAttrs = {attrPXInvT, attrPYInvT, attrPZInvT, attrNXInvT, attrNYInvT, attrNZInvT};                      \
    faceAttrs.mSwapSTAttrs = {attrPXSwapST, attrPYSwapST, attrPZSwapST, attrNXSwapST, attrNYSwapST, attrNZSwapST};        \
    faceAttrs.mRotAttrs = {attrPXRot, attrPYRot, attrPZRot, attrNXRot, attrNYRot, attrNZRot};                             \
    faceAttrs.mRotCntrAttrs = {attrPXRotCntr, attrPYRotCntr, attrPZRotCntr, attrNXRotCntr, attrNYRotCntr, attrNZRotCntr}; \
    faceAttrs.mOffsetAttrs = {attrPXOffset, attrPYOffset, attrPZOffset, attrNXOffset, attrNYOffset, attrNZOffset};        \
    faceAttrs.mScaleAttrs = {attrPXScale, attrPYScale, attrPZScale, attrNXScale, attrNYScale, attrNZScale};

void updateTriplanarMap(scene_rdl2::rdl2::Shader* map,
                        const ispc::TEXTURE_GammaMode gammaMode,
                        const int randomSeed,
                        const bool randomizeFlip,
                        const bool randomizeOffset,
                        const bool randomizeRotation,
                        const float transitionWidth,
                        const int numTextures,
                        const scene_rdl2::rdl2::SceneObject* projectorObject,
                        const ispc::PROJECTION_Mode projectionMode,
                        const scene_rdl2::math::Mat4d &projectionMatrix,
                        const ispc::PROJECTION_TransformOrder transformOrder,
                        const ispc::PROJECTION_RotationOrder rotationOrder,
                        const scene_rdl2::math::Vec3f& translation,
                        const scene_rdl2::math::Vec3f& rotation,
                        const scene_rdl2::math::Vec3f& scale,
                        const TriplanarFaceAttrs& faceAttrs,
                        const scene_rdl2::math::Color& fatalColor,
                        scene_rdl2::rdl2::ShaderLogEventRegistry& logEventRegistry,
                        ispc::PROJECTION_TriplanarData& outputIspcData,
                        std::array<std::unique_ptr<projection::TriplanarTexture>, 6>& outputTriplanarTextures,
                        std::unique_ptr<moonray::shading::Xform>& outputProjectorXform);

void fillTriplanarTextures(moonray::shading::TLState* tls,
                           const moonray::shading::State& state, 
                           const int* textureIndices,
                           const std::array<std::unique_ptr<TriplanarTexture>, 6>& outputTriplanarTextures,
                           const float transitionWidthPower,
                           const scene_rdl2::math::Vec3f& normal,
                           const scene_rdl2::math::Vec3f& pos,
                           const scene_rdl2::math::Vec3f& pos_ddx,
                           const scene_rdl2::math::Vec3f& pos_ddy,
                           const scene_rdl2::math::Vec3f& pos_ddz,
                           scene_rdl2::math::Vec3f& outputBlending,
                           scene_rdl2::math::Color* outputColors);

void fillTriplanarNormalTextures(const int normalMapEncoding,
                                 moonray::shading::TLState* tls,
                                 const moonray::shading::State& state, 
                                 const int* textureIndices,
                                 const std::array<std::unique_ptr<TriplanarTexture>, 6>& outputTriplanarTextures,
                                 const float transitionWidthPower,
                                 const scene_rdl2::math::Vec3f& normal,
                                 const scene_rdl2::math::Vec3f& pos,
                                 const scene_rdl2::math::Vec3f& pos_ddx,
                                 const scene_rdl2::math::Vec3f& pos_ddy,
                                 const scene_rdl2::math::Vec3f& pos_ddz,
                                 scene_rdl2::math::Vec3f& outputBlending,
                                 scene_rdl2::math::Vec3f* outputNormals,
                                 float* outputNormalLengths);

} // projection
} // moonshine


