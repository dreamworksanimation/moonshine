// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ProjectCameraMap_v2.cc

#include "attributes.cc"
#include "ProjectCameraMap_v2_ispc_stubs.h"

#include <moonray/map/primvar/Primvar.h>
#include <moonshine/map/projection/ProjectionUtil.h>

#include <moonray/common/mcrt_macros/moonray_static_check.h>
#include <moonray/rendering/shading/BasicTexture.h>
#include <moonray/rendering/shading/MapApi.h>

#include <memory>

using namespace moonshine;
using namespace scene_rdl2::math;

static ispc::PROJECTION_StaticData sStaticProjectCameraMapData;

//----------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(ProjectCameraMap_v2, scene_rdl2::rdl2::Map)

public:
    ProjectCameraMap_v2(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name);
    ~ProjectCameraMap_v2() override;
    void update() override;

private:
    static void sample(const scene_rdl2::rdl2::Map *self, moonray::shading::TLState *tls,
                       const moonray::shading::State &state, Color *sample);

    ispc::ProjectCameraMap_v2 mIspc;
    std::unique_ptr<moonray::shading::Xform> mXform;
    std::unique_ptr<moonray::shading::BasicTexture> mTexture;

RDL2_DSO_CLASS_END(ProjectCameraMap_v2)

//----------------------------------------------------------------------------

ProjectCameraMap_v2::ProjectCameraMap_v2(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name):
    Parent(sceneClass, name)
{
    mSampleFunc = ProjectCameraMap_v2::sample;
    mSampleFuncv = (scene_rdl2::rdl2::SampleFuncv) ispc::ProjectCameraMap_v2_getSampleFunc();

    mIspc.mHasValidProjector = false;

    // Store keys to shader data
    mIspc.mRefPKey = moonray::shading::StandardAttributes::sRefP;
    mIspc.mRefNKey = moonray::shading::StandardAttributes::sRefN;

    mIspc.mStaticData = (ispc::PROJECTION_StaticData*)&sStaticProjectCameraMapData;

    const scene_rdl2::rdl2::SceneVariables &sv = getSceneClass().getSceneContext()->getSceneVariables();
    asCpp(mIspc.mFatalColor) = sv.get(scene_rdl2::rdl2::SceneVariables::sFatalColor);

    mTexture = std::make_unique<moonray::shading::BasicTexture>(this, sLogEventRegistry);
    mIspc.mTexture = &mTexture->getBasicTextureData();

    // Set projection error messages and fatal color
    projection::initLogEvents(*mIspc.mStaticData, sLogEventRegistry, this);
}

ProjectCameraMap_v2::~ProjectCameraMap_v2()
{
}

void
ProjectCameraMap_v2::update()
{
    // Update BasicTexture and make sure it is valid
    if (hasChanged(attrTexture) || hasChanged(attrGamma)) {
        std::string errorStr;
        if (!mTexture->update(get(attrTexture),
                              static_cast<ispc::TEXTURE_GammaMode>(get(attrGamma)),
                              moonray::shading::WrapType::Clamp,      // wrapS
                              moonray::shading::WrapType::Clamp,      // wrapT
                              false,                      // use default color
                              sBlack,                     // default color
                              asCpp(mIspc.mFatalColor),   // fatal color
                              errorStr)) {
            fatal(errorStr);
            return;
        }
    }

    // Construct Xform object with custom camera and window that matches the
    // image dimensions and pixel aspect ratio of the texture to be projected
    const scene_rdl2::rdl2::SceneObject* projectorObject = get(attrProjector);
    const scene_rdl2::rdl2::Camera* projectorCamera = (projectorObject) ?
                                          projectorObject->asA<scene_rdl2::rdl2::Camera>() :
                                          nullptr;
    mIspc.mHasValidProjector = (projectorObject != nullptr);

    // exit early ?
    if (!mIspc.mHasValidProjector) {
        return;
    }

    // determine aspect ratio
    float aspectRatio;
    if (get(attrAspectRatioSource) == ispc::ASPECT_SOURCE_TEXTURE) {
        if (hasChanged(attrTexture)) {
            // compute 'screen window' based on the texture's dimensions
            // and pixel aspect ratio
            int width;
            int height;
            mTexture->getDimensions(width, height);
            const float imageAspectRatio = static_cast<float>(width)/height;
            const float pixelAspectRatio = mTexture->getPixelAspectRatio();
            aspectRatio = imageAspectRatio * pixelAspectRatio;
        }
    } else { // ispc::ASPECT_SOURCE_CUSTOM
        aspectRatio = get(attrCustomAspectRatio);
    }

    // build transform
    if (hasChanged(attrTexture) || hasChanged(attrProjector) || hasChanged(attrAspectRatioSource)) {
        // build window
        std::array<float, 4> window;
        if (aspectRatio >= 1.0f) {
            aspectRatio = rcp(aspectRatio);
            window = { -1.0f, aspectRatio, 1.0f, -aspectRatio};
        } else {
            window = { -aspectRatio, 1.0f, aspectRatio, -1.0f};
        }

        mXform = std::make_unique<moonray::shading::Xform>(this, nullptr, projectorCamera, &window);
        mIspc.mXform = mXform->getIspcXform();
    }

    // map [-1, 1] -> [0, 1]
    static const Xform3f s2uv(Vec3f(0.5f, 0.0f, 0.0f),
                                    Vec3f(0.0f, 0.5f, 0.0f),
                                    Vec3f(0.0f, 0.0f, 1.0f),
                                    Vec3f(0.5f, 0.5f, 0.0f));

    asCpp(mIspc.s2uv) = s2uv;

    // declare needed attributes
    if (hasChanged(attrUseReferenceSpace)) {
        mRequiredAttributes.clear();
        mOptionalAttributes.clear();
        if (get(attrUseReferenceSpace)) {
            mRequiredAttributes.push_back(mIspc.mRefPKey);
            if (!get(attrProjectOnBackFaces)) {
                mOptionalAttributes.push_back(mIspc.mRefNKey);
            }
        }
    }
}

void
ProjectCameraMap_v2::sample(const scene_rdl2::rdl2::Map *self, moonray::shading::TLState *tls,
                            const moonray::shading::State &state, Color *sample)
{
    ProjectCameraMap_v2 const *me = static_cast<ProjectCameraMap_v2 const *>(self);

    *sample = Color(0.0f, 0.0f, 0.0f);

    if (!me->mIspc.mHasValidProjector) {
        // Log missing projector data message
        moonray::shading::logEvent(me, me->mIspc.mStaticData->sErrorMissingProjector);
        return;
    }

    // Retrieve position in screen space
    ispc::PRIMVAR_Input_Source_Mode inputSourceMode;
    if (me->get(attrUseReferenceSpace)) {
        inputSourceMode = ispc::INPUT_SOURCE_MODE_REF_P_REF_N;
    } else {
        inputSourceMode = ispc::INPUT_SOURCE_MODE_P_N;
    }

    Vec3f P_s, dPdx_s, dPdy_s, dPdz_s, inputPosition;
    if (!primvar::getPosition(tls, state,
                              inputSourceMode,
                              inputPosition,
                              me->mXform.get(),
                              ispc::SHADING_SPACE_SCREEN,
                              me->mIspc.mRefPKey,
                              P_s, dPdx_s, dPdy_s, dPdz_s)) {
        // Log missing ref_P data message
        moonray::shading::logEvent(me, me->mIspc.mStaticData->sErrorMissingRefP);
        *sample = asCpp(me->mIspc.mStaticData->sFatalColor);
        return;
    }

    // check to see if the point is within the frustum
    if (me->get(attrBlackOutsideProjection) &&
        (P_s.x < -1.f || P_s.x > 1.f || P_s.y < -1.f || P_s.y > 1.f)) {
        return;
    }

    // Retrieve normal in projection camera's space
    Vec3f N_c(0.f, 0.f, -1.f); // default to back facing
    if (!me->get(attrProjectOnBackFaces)) {
        Vec3f inputNormal;
        if (!primvar::getNormal(tls, state,
                                inputSourceMode,
                                inputNormal,
                                me->mXform.get(),
                                ispc::SHADING_SPACE_CAMERA,
                                me->mIspc.mRefPKey,
                                me->mIspc.mRefNKey,
                                N_c)) {
            // Log missing ref_N data message
            moonray::shading::logEvent(me, me->mIspc.mStaticData->sErrorMissingRefN);
            *sample = asCpp(me->mIspc.mStaticData->sFatalColor);
            return;
        }
    }

    const bool isFrontFacing = N_c.z > 0.0f;

    if (me->get(attrProjectOnBackFaces) || isFrontFacing) {
        // Transform from screen space to UV space (which maps from [-1,1] to [0,1] on xy),
        // 'U' is our UV coordinate
        const Vec3f U     = transformPoint(asCpp(me->mIspc.s2uv), P_s);
        const Vec3f dUdx  = transformVector(asCpp(me->mIspc.s2uv), dPdx_s);
        const Vec3f dUdy  = transformVector(asCpp(me->mIspc.s2uv), dPdy_s);

        float derivatives[4];
        derivatives[0] = dUdx.x;    // dsdx
        derivatives[1] = dUdx.y;    // dtdx
        derivatives[2] = dUdy.x;    // dsdy
        derivatives[3] = dUdy.y;    // dtdy

        const Vec2f st(U.x, U.y);

        // sample the texture
        const Color4 tx = me->mTexture->sample(tls, state, st, derivatives);
        if (me->get(attrAlphaOnly)) {
            const Color alpha(tx.a, tx.a, tx.a);
            *sample = alpha;
        } else {
            Color rgb(tx);
            if (me->get(attrUnpremultiply) && tx.a > 0.001f && tx.a < 1.0f) {
                rgb.r /= tx.a;
                rgb.g /= tx.a;
                rgb.b /= tx.a;
            }
            *sample = rgb;
        }
    }
}

