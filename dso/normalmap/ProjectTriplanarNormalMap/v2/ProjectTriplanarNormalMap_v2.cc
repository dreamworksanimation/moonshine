// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.cc"
#include "ProjectTriplanarNormalMap_v2_ispc_stubs.h"

#include <moonshine/map/projection/ProjectionUtil.h>
#include <moonshine/map/projection/TriplanarTexture.h>
#include <moonray/map/primvar/Primvar.h>

#include <moonray/common/mcrt_macros/moonray_static_check.h>
#include <moonray/rendering/shading/BasicTexture.h>
#include <moonray/rendering/shading/MapApi.h>
#include <scene_rdl2/scene/rdl2/rdl2.h>
#include <scene_rdl2/render/util/stdmemory.h>
#include <scene_rdl2/common/math/Constants.h>
#include <scene_rdl2/render/util/Random.h>

using namespace moonshine;
using namespace scene_rdl2::math;

static ispc::PROJECTION_StaticData sStaticProjectTriplanarNormalMap_v2Data;

RDL2_DSO_CLASS_BEGIN(ProjectTriplanarNormalMap_v2, NormalMap)

public:
ProjectTriplanarNormalMap_v2(const SceneClass& sceneClass, const std::string& name);
    ~ProjectTriplanarNormalMap_v2() override;
    void update() override;

private:
    static void sampleNormal(const NormalMap* self,
                             moonray::shading::TLState* tls,
                             const moonray::shading::State& state,
                             Vec3f* sample);

    ispc::ProjectTriplanarNormalMap_v2 mIspc; // must be first member
    projection::TriplanarFaceAttrs mFaceAttrs;
    std::array<std::unique_ptr<projection::TriplanarTexture>, 6> mTriplanarTextures;
    std::unique_ptr<moonray::shading::Xform> mProjectorXform;
    std::unique_ptr<moonray::shading::Xform> mObjXform;

RDL2_DSO_CLASS_END(ProjectTriplanarNormalMap_v2)

//----------------------------------------------------------------------------

ProjectTriplanarNormalMap_v2::ProjectTriplanarNormalMap_v2(const SceneClass& sceneClass, const std::string& name)
    : Parent(sceneClass, name)
{
    mSampleNormalFunc = ProjectTriplanarNormalMap_v2::sampleNormal;
    mSampleNormalFuncv = (SampleNormalFuncv) ispc::ProjectTriplanarNormalMap_v2_getSampleFunc();

    // Store keys to shader data
    mIspc.mTriplanarData.mRefPKey = moonray::shading::StandardAttributes::sRefP;
    mIspc.mTriplanarData.mRefNKey = moonray::shading::StandardAttributes::sRefN;

    mIspc.mStaticData = (ispc::PROJECTION_StaticData*)&sStaticProjectTriplanarNormalMap_v2Data;

    // Collect common per-face parameters into arrays for effecient processing
    ASSIGN_TRIPLANAR_FACE_ATTRS(mFaceAttrs);
    
    // Set projection error messages and fatal color
    projection::initLogEvents(*mIspc.mStaticData,
                              mLogEventRegistry,
                              this);
}

ProjectTriplanarNormalMap_v2::~ProjectTriplanarNormalMap_v2()
{
}

void
ProjectTriplanarNormalMap_v2::update()
{
    projection::updateTriplanarMap(this,
                                   ispc::TEXTURE_GAMMA_OFF,
                                   get(attrRandomSeed),
                                   get(attrRandomFlip),
                                   get(attrRandomOffset),
                                   get(attrRandomRot),
                                   get(attrTransitionWidth),
                                   get(attrNumTextures),
                                   get(attrProjector),
                                   static_cast<ispc::PROJECTION_Mode>(get(attrProjectionMode)),
                                   get(attrProjectionMatrix),
                                   static_cast<ispc::PROJECTION_TransformOrder>(get(attrTRSOrder)),
                                   static_cast<ispc::PROJECTION_RotationOrder>(get(attrRotationOrder)),
                                   get(attrTranslate),
                                   get(attrRotate),
                                   get(attrScale),
                                   mFaceAttrs,
                                   asCpp(mIspc.mStaticData->sFatalColor),
                                   mLogEventRegistry,
                                   mIspc.mTriplanarData,
                                   mTriplanarTextures,
                                   mProjectorXform);

    // Use reference space
    if (hasChanged(attrInputSource)) {
        mRequiredAttributes.clear();
        mOptionalAttributes.clear();
        if (get(attrInputSource) == ispc::INPUT_SOURCE_MODE_REF_P_REF_N) {
            mRequiredAttributes.push_back(mIspc.mTriplanarData.mRefPKey);
            mOptionalAttributes.push_back(mIspc.mTriplanarData.mRefNKey);
        }
    }

    // Construct Xform for space of object being rendered to transform texture normals into
    mObjXform = fauxstd::make_unique<moonray::shading::Xform>(this, nullptr, nullptr, nullptr);
    mIspc.mTriplanarData.mObjXform = mObjXform->getIspcXform();

    // Get whether or not the normals have been reversed
    mOptionalAttributes.push_back(moonray::shading::StandardAttributes::sReversedNormals);
    mIspc.mTriplanarData.mReversedNormalsIndx = moonray::shading::StandardAttributes::sReversedNormals.getIndex();
}

void
ProjectTriplanarNormalMap_v2::sampleNormal(const NormalMap* self,
                                           moonray::shading::TLState* tls,
                                           const moonray::shading::State& state,
                                           Vec3f* sample) {

    const ProjectTriplanarNormalMap_v2 *me = static_cast<ProjectTriplanarNormalMap_v2 const *>(self);

    *sample = state.getN();

    if (!me->mIspc.mTriplanarData.mHasValidProjector) {
        // Log missing projector data message
        moonray::shading::logEvent(me, tls, me->mIspc.mStaticData->sErrorMissingProjector);
        return;
    }

    // Retrieve position and normal and transform into
    // the object space of the projector
    const ispc::PROJECTION_TriplanarData& data = me->mIspc.mTriplanarData;

    ispc::PRIMVAR_Input_Source_Mode inputSourceMode =
        static_cast<ispc::PRIMVAR_Input_Source_Mode>(me->get(attrInputSource));

    Vec3f pos, pos_ddx, pos_ddy, pos_ddz, inputPosition;
    if (inputSourceMode == ispc::INPUT_SOURCE_MODE_ATTR) {
        inputPosition = evalVec3f(me, attrInputPosition, tls, state);
    }
    if (!primvar::getPosition(tls, state,
                              inputSourceMode,
                              inputPosition,
                              me->mProjectorXform.get(),
                              ispc::SHADING_SPACE_OBJECT,
                              data.mRefPKey,
                              pos, pos_ddx, pos_ddy, pos_ddz)) {
        // Log missing ref_P data message
        moonray::shading::logEvent(me, tls, me->mIspc.mStaticData->sErrorMissingRefP);
        return;
    }
 
    Vec3f normal, inputNormal;
    if (inputSourceMode == ispc::INPUT_SOURCE_MODE_ATTR) {
        inputNormal = evalVec3f(me, attrInputNormal, tls, state);
    }
    if (!primvar::getNormal(tls, state,
                            inputSourceMode,
                            inputNormal,
                            me->mProjectorXform.get(),
                            ispc::SHADING_SPACE_OBJECT,
                            data.mRefPKey,
                            data.mRefNKey,
                            normal)) {
        // Log missing ref_N data message
        moonray::shading::logEvent(me, tls, me->mIspc.mStaticData->sErrorMissingRefN);
        return;
    }

    Vec3f blending;
    Vec3f oNormal[6];
    float tNormalLength[6];
    fillTriplanarNormalTextures(me->get(attrNormalEncoding),
                                tls, state,
                                me->mIspc.mTriplanarData.mTextureIndices,
                                me->mTriplanarTextures,
                                data.mTransitionWidthPower,
                                normal,
                                pos, pos_ddx, pos_ddy, pos_ddz,
                                blending,
                                oNormal, tNormalLength);

    // Blend directions
    const Vec3f bNormal = ((normal.x > 0.0f) ? oNormal[0] : oNormal[3]) * blending.x +
                          ((normal.y > 0.0f) ? oNormal[1] : oNormal[4]) * blending.y +
                          ((normal.z > 0.0f) ? oNormal[2] : oNormal[5]) * blending.z;

    const float bNormalLength =
        ((normal.x > 0.0f) ? tNormalLength[0] : tNormalLength[3]) * blending.x +
        ((normal.y > 0.0f) ? tNormalLength[1] : tNormalLength[4]) * blending.y +
        ((normal.z > 0.0f) ? tNormalLength[2] : tNormalLength[5]) * blending.z;

    Vec3f rNormal;
    if (inputSourceMode == ispc::INPUT_SOURCE_MODE_REF_P_REF_N) {
        // Transform blended normal from the object space of the
        // projector to world space
        const Vec3f pNormal = me->mProjectorXform->transformNormal(
                ispc::SHADING_SPACE_OBJECT,
                ispc::SHADING_SPACE_WORLD,
                state,
                bNormal);

        // Transform blended normal from world space to the
        // space of the rendered object and then to render space
        rNormal = me->mObjXform->transformNormal(
            ispc::SHADING_SPACE_OBJECT,
            ispc::SHADING_SPACE_RENDER,
            state,
            pNormal);
    } else {
        // Transform blended normal from the object space of the
        // projector to render space
        rNormal = me->mProjectorXform->transformNormal(
            ispc::SHADING_SPACE_OBJECT,
            ispc::SHADING_SPACE_RENDER,
            state,
            bNormal);
    }

    // bNormalLength is used to preserve the original length of the normal,
    // which is necessary for the Toksvig mapping technique which recalculates
    // the roughness value based on the length of the input normal in the Dwa materials.
    // adjust the normal as necessary with to avoid black
    // artifacts at oblique viewing angles.
    rNormal = bNormalLength * normalize(rNormal);
    *sample = rNormal;
}

