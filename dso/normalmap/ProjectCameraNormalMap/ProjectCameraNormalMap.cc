// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ProjectCameraNormalMap.cc

#include "attributes.cc"
#include "ProjectCameraNormalMap_ispc_stubs.h"

#include <moonray/map/primvar/Primvar.h>
#include <moonshine/map/projection/ProjectionUtil.h>

#include <moonray/common/mcrt_macros/moonray_static_check.h>
#include <moonray/rendering/shading/BasicTexture.h>
#include <moonray/rendering/shading/MapApi.h>
#include <scene_rdl2/render/util/stdmemory.h>

using namespace moonshine;
using namespace scene_rdl2::math;

static ispc::PROJECTION_StaticData sStaticProjectCameraMapData;

//----------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(ProjectCameraNormalMap, NormalMap)

public:
    ProjectCameraNormalMap(const SceneClass &sceneClass, const std::string &name);
    ~ProjectCameraNormalMap() override;
    void update() override;

private:
    static void sampleNormal(const NormalMap *self, moonray::shading::TLState *tls,
                             const moonray::shading::State &state, Vec3f *sample);

    ispc::ProjectCameraNormalMap mIspc;
    std::unique_ptr<moonray::shading::Xform> mXform;
    std::unique_ptr<moonray::shading::BasicTexture> mTexture;

RDL2_DSO_CLASS_END(ProjectCameraNormalMap)

//----------------------------------------------------------------------------

ProjectCameraNormalMap::ProjectCameraNormalMap(const SceneClass &sceneClass, const std::string &name):
    Parent(sceneClass, name)
{
    mSampleNormalFunc = ProjectCameraNormalMap::sampleNormal;
    mSampleNormalFuncv = (SampleNormalFuncv) ispc::ProjectCameraNormalMap_getSampleFunc();

    mIspc.mHasValidProjector = false;

    // Store keys to shader data
    mIspc.mRefPKey = moonray::shading::StandardAttributes::sRefP;
    mIspc.mRefNKey = moonray::shading::StandardAttributes::sRefN;

    mIspc.mStaticData = (ispc::PROJECTION_StaticData*)&sStaticProjectCameraMapData;

    mTexture = fauxstd::make_unique<moonray::shading::BasicTexture>(this, mLogEventRegistry);
    mIspc.mTexture = &mTexture->getBasicTextureData();

    // Set projection error messages and fatal color
    projection::initLogEvents(*mIspc.mStaticData,
                              mLogEventRegistry,
                              this);
}

ProjectCameraNormalMap::~ProjectCameraNormalMap()
{
}

void
ProjectCameraNormalMap::update()
{
    // Update BasicTexture and make sure it is valid
    if (hasChanged(attrTexture)) {
        std::string errorStr;
        if (!mTexture->update(get(attrTexture),
                              ispc::TEXTURE_GAMMA_OFF,
                              moonray::shading::WrapType::Clamp, // wrapS
                              moonray::shading::WrapType::Clamp, // wrapT
                              true,                              // use default color
                              Color(0.5f, 0.5f, 1.0f),           // default color
                              sBlack,                            // fatal color
                              errorStr)) {
            fatal(errorStr);
            return;
        }
    }

    // Construct Xform object with custom camera and window that matches the
    // image dimensions and pixel aspect ratio of the texture to be projected
    const SceneObject* projectorObject = get(attrProjector);
    const Camera* projectorCamera = (projectorObject) ?
                                          projectorObject->asA<Camera>() :
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

        mXform = fauxstd::make_unique<moonray::shading::Xform>(this, nullptr, projectorCamera, &window);
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

    // Get whether or not the normals have been reversed
    mOptionalAttributes.push_back(moonray::shading::StandardAttributes::sReversedNormals);
    mIspc.mReversedNormalsIndx = moonray::shading::StandardAttributes::sReversedNormals.getIndex();
}

void
ProjectCameraNormalMap::sampleNormal(const NormalMap *self,
                                     moonray::shading::TLState *tls,
                                     const moonray::shading::State &state,
                                     Vec3f *sample)
{
    ProjectCameraNormalMap const *me = static_cast<ProjectCameraNormalMap const *>(self);

    *sample = state.getN();

    if (!me->mIspc.mHasValidProjector) {
        // Log missing projector data message
        moonray::shading::logEvent(me, tls, me->mIspc.mStaticData->sErrorMissingProjector);
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
        moonray::shading::logEvent(me, tls, me->mIspc.mStaticData->sErrorMissingRefP);
        return;
    }

    // check to see if the point is within the frustum
    if (P_s.x < -1.f || P_s.x > 1.f || P_s.y < -1.f || P_s.y > 1.f) {
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
            moonray::shading::logEvent(me, tls, me->mIspc.mStaticData->sErrorMissingRefN);
            return;
        }
    }

    const bool isFrontFacing = N_c.z > 0.0f;
    Vec3f normal = state.getN();
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
        normal = Vec3f(tx.r, tx.g, tx.b);
    }

    if (std::isnan(normal.x) ||
        std::isnan(normal.y) ||
        std::isnan(normal.z) ||
        !std::isfinite(normal.x) ||
        !std::isfinite(normal.y) ||
        !std::isfinite(normal.z) ||
        isZero(normal)) {

        *sample = state.getN();
        return;
    }

    // Re-center sampled normal from [0, 1] --> [-1, 1]
    // z is usually encoded from [0.5, 1.0] so that this re-centering
    // keeps it positive (in [0, 1])
    const int normalMapEncoding = me->get(attrNormalEncoding);
    if (normalMapEncoding == 0) { // [0,1]
        normal = 2.0f * normal - Vec3f(1.0f, 1.0f, 1.0f);
    }

    // Transform from tangent space to shade space
    bool reversedNormals = false;
    if (state.isProvided(moonray::shading::StandardAttributes::sReversedNormals)) {
        reversedNormals = state.getAttribute(moonray::shading::StandardAttributes::sReversedNormals);
    }
    const scene_rdl2::math::Vec3f statedPds = reversedNormals ? state.getdPds() * -1.0f : state.getdPds() * 1.0f;
    const ReferenceFrame frame(state.getN(), scene_rdl2::math::normalize(statedPds));
    const Vec3f renderN = frame.localToGlobal(normal);

    // Don't normalize vector, need unnormalized length for
    // Toksvig normal anti-aliasing technique in Dwa materials.
    *sample = renderN;
}

