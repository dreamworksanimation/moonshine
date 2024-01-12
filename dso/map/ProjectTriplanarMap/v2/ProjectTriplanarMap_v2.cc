// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.cc"
#include "ProjectTriplanarMap_v2_ispc_stubs.h"

#include <moonray/map/primvar/Primvar.h>
#include <moonshine/map/projection/TriplanarTexture.h>
#include <moonshine/map/projection/ProjectionUtil.h>

#include <moonray/common/mcrt_macros/moonray_static_check.h>
#include <moonray/rendering/shading/MapApi.h>

using namespace moonshine;
using namespace scene_rdl2::math;

static ispc::PROJECTION_StaticData sStaticProjectTriplanarMap_v2_Data;

RDL2_DSO_CLASS_BEGIN(ProjectTriplanarMap_v2, scene_rdl2::rdl2::Map)

public:
ProjectTriplanarMap_v2(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name);
    ~ProjectTriplanarMap_v2() override;
    void update() override;

private:
    static void sample(const scene_rdl2::rdl2::Map* self, moonray::shading::TLState* tls,
                       const moonray::shading::State& state, Color* sample);


    ispc::ProjectTriplanarMap mIspc; // must be first member

    projection::TriplanarFaceAttrs mFaceAttrs;

    std::array<std::unique_ptr<projection::TriplanarTexture>, 6> mTriplanarTextures;
    std::unique_ptr<moonray::shading::Xform> mProjectorXform;

RDL2_DSO_CLASS_END(ProjectTriplanarMap_v2)

//----------------------------------------------------------------------------

ProjectTriplanarMap_v2::ProjectTriplanarMap_v2(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name)
    : Parent(sceneClass, name)
{
    mSampleFunc = ProjectTriplanarMap_v2::sample;
    mSampleFuncv = (scene_rdl2::rdl2::SampleFuncv) ispc::ProjectTriplanarMap_v2_getSampleFunc();

    // Store keys to shader data
    mIspc.mTriplanarData.mRefPKey = moonray::shading::StandardAttributes::sRefP;
    mIspc.mTriplanarData.mRefNKey = moonray::shading::StandardAttributes::sRefN;

    mIspc.mStaticData = (ispc::PROJECTION_StaticData*)&sStaticProjectTriplanarMap_v2_Data;

    // Collect common per-face parameters into arrays for effecient processing
    ASSIGN_TRIPLANAR_FACE_ATTRS(mFaceAttrs);

    // Set projection error messages and fatal color
    projection::initLogEvents(*mIspc.mStaticData, sLogEventRegistry, this);
}

ProjectTriplanarMap_v2::~ProjectTriplanarMap_v2()
{
}

void
ProjectTriplanarMap_v2::update()
{
    projection::updateTriplanarMap(this,
                                   static_cast<ispc::TEXTURE_GammaMode>(get(attrGamma)),
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
                                   sLogEventRegistry,
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
}

void
ProjectTriplanarMap_v2::sample(const scene_rdl2::rdl2::Map* self, moonray::shading::TLState* tls, const moonray::shading::State& state, Color* sample) {
    const ProjectTriplanarMap_v2 *me = static_cast<ProjectTriplanarMap_v2 const *>(self);

    if (!me->mIspc.mTriplanarData.mHasValidProjector) {
        // Log missing projector data message
        moonray::shading::logEvent(me, me->mIspc.mStaticData->sErrorMissingProjector);
        *sample = asCpp(me->mIspc.mStaticData->sFatalColor);
        return;
    }

    const ispc::PROJECTION_TriplanarData& data = me->mIspc.mTriplanarData;

    // Retrieve position and normal and transform into
    // the object space of the projector
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
        moonray::shading::logEvent(me, me->mIspc.mStaticData->sErrorMissingRefP);
        *sample = asCpp(me->mIspc.mStaticData->sFatalColor);
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
        moonray::shading::logEvent(me, me->mIspc.mStaticData->sErrorMissingRefN);
        *sample = asCpp(me->mIspc.mStaticData->sFatalColor);
        return;
    }

    Vec3f blending;
    Color tx[6];
    projection::fillTriplanarTextures(tls, state,
                                      me->mIspc.mTriplanarData.mTextureIndices,
                                      me->mTriplanarTextures,
                                      me->mIspc.mTriplanarData.mTransitionWidthPower,
                                      normal,
                                      pos, pos_ddx, pos_ddy, pos_ddz,
                                      blending,
                                      tx);

    *sample =
        ((normal.x > 0.0f) ? tx[0] : tx[3]) * blending.x +
        ((normal.y > 0.0f) ? tx[1] : tx[4]) * blending.y +
        ((normal.z > 0.0f) ? tx[2] : tx[5]) * blending.z;
}

