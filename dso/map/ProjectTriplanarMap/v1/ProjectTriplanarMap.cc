// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.cc"
#include "ProjectTriplanarMap_ispc_stubs.h"

#include <moonray/map/primvar/Primvar.h>
#include <moonshine/map/projection/TriplanarTexture.h>
#include <moonshine/map/projection/ProjectionUtil.h>

#include <moonray/common/mcrt_macros/moonray_static_check.h>
#include <moonray/rendering/shading/MapApi.h>

using namespace moonshine;
using namespace scene_rdl2::math;

static ispc::PROJECTION_StaticData sStaticProjectTriplanarMapData;

RDL2_DSO_CLASS_BEGIN(ProjectTriplanarMap, Map)

public:
ProjectTriplanarMap(const SceneClass& sceneClass, const std::string& name);
    ~ProjectTriplanarMap() override;
    void update() override;

private:
    static void sample(const Map* self, moonray::shading::TLState* tls,
                       const moonray::shading::State& state, Color* sample);


    ispc::ProjectTriplanarMap mIspc; // must be first member

    projection::TriplanarFaceAttrs mFaceAttrs;

    std::array<std::unique_ptr<projection::TriplanarTexture>, 6> mTriplanarTextures;
    std::unique_ptr<moonray::shading::Xform> mProjectorXform;

RDL2_DSO_CLASS_END(ProjectTriplanarMap)

//----------------------------------------------------------------------------

ProjectTriplanarMap::ProjectTriplanarMap(const SceneClass& sceneClass, const std::string& name)
    : Parent(sceneClass, name)
{
    mSampleFunc = ProjectTriplanarMap::sample;
    mSampleFuncv = (SampleFuncv) ispc::ProjectTriplanarMap_getSampleFunc();

    // Store keys to shader data
    mIspc.mTriplanarData.mRefPKey = moonray::shading::StandardAttributes::sRefP;
    mIspc.mTriplanarData.mRefNKey = moonray::shading::StandardAttributes::sRefN;

    mIspc.mStaticData = (ispc::PROJECTION_StaticData*)&sStaticProjectTriplanarMapData;

    // Collect common per-face parameters into arrays for effecient processing
    ASSIGN_TRIPLANAR_FACE_ATTRS(mFaceAttrs);

    // Set projection error messages and fatal color
    projection::initLogEvents(*mIspc.mStaticData,
                              mLogEventRegistry,
                              this);
}

ProjectTriplanarMap::~ProjectTriplanarMap()
{
}

void
ProjectTriplanarMap::update()
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
                                   mLogEventRegistry,
                                   mIspc.mTriplanarData,
                                   mTriplanarTextures,
                                   mProjectorXform);

    // Use reference space
    if (hasChanged(attrUseReferenceSpace)) {
        mRequiredAttributes.clear();
        mOptionalAttributes.clear();
        if (get(attrUseReferenceSpace)) {
            mRequiredAttributes.push_back(mIspc.mTriplanarData.mRefPKey);
            mOptionalAttributes.push_back(mIspc.mTriplanarData.mRefNKey);
        }
    }
}

void
ProjectTriplanarMap::sample(const Map* self, moonray::shading::TLState* tls, const moonray::shading::State& state, Color* sample) {
    const ProjectTriplanarMap *me = static_cast<ProjectTriplanarMap const *>(self);

    if (!me->mIspc.mTriplanarData.mHasValidProjector) {
        // Log missing projector data message
        moonray::shading::logEvent(me, tls, me->mIspc.mStaticData->sErrorMissingProjector);
        *sample = asCpp(me->mIspc.mStaticData->sFatalColor);
        return;
    }

    const ispc::PROJECTION_TriplanarData& data = me->mIspc.mTriplanarData;

    // Retrieve position and normal and transform into
    // the object space of the projector
    ispc::PRIMVAR_Input_Source_Mode inputSourceMode;
    if (me->get(attrUseReferenceSpace)) {
        inputSourceMode = ispc::INPUT_SOURCE_MODE_REF_P_REF_N;
    } else {
        inputSourceMode = ispc::INPUT_SOURCE_MODE_P_N;
    }

    Vec3f pos, pos_ddx, pos_ddy, pos_ddz, inputPosition;
    if (!primvar::getPosition(tls, state,
                              inputSourceMode,
                              inputPosition,
                              me->mProjectorXform.get(),
                              ispc::SHADING_SPACE_OBJECT,
                              data.mRefPKey,
                              pos, pos_ddx, pos_ddy, pos_ddz)) {
        // Log missing ref_P data message
        moonray::shading::logEvent(me, tls, me->mIspc.mStaticData->sErrorMissingRefP);
        *sample = asCpp(me->mIspc.mStaticData->sFatalColor);
        return;
    }
 
    Vec3f normal, inputNormal;
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

