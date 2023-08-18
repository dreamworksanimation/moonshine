// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.cc"
#include "ProjectSphericalMap_ispc_stubs.h"

#include <moonray/map/primvar/Primvar.h>
#include <moonshine/map/projection/ProjectionUtil.h>

#include <moonray/common/mcrt_macros/moonray_static_check.h>
#include <moonray/rendering/shading/MapApi.h>

using namespace moonshine;
using namespace scene_rdl2::math;

static ispc::PROJECTION_StaticData sStaticProjectSphericalMapData;

//----------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(ProjectSphericalMap, scene_rdl2::rdl2::Map)

public:
    ProjectSphericalMap(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name);
    ~ProjectSphericalMap() override;
    void update() override;

private:
    static void sample(const scene_rdl2::rdl2::Map* self, moonray::shading::TLState* tls, const moonray::shading::State& state, Color* sample);

    ispc::ProjectSphericalMap mIspc;
    std::unique_ptr<moonray::shading::Xform> mXform;

RDL2_DSO_CLASS_END(ProjectSphericalMap)

//----------------------------------------------------------------------------

namespace {
// Spherical projection - convert cartesian coordinates to polar coords
Vec3f sphereProjection(const Vec3f& point)
{
    Vec3f result(0.0f);

    // Set projection t
    float len = point.length();
    if (!isZero(len)) {
        result.y = scene_rdl2::math::acos(point.y / len) / sPi;
    }

    // Set projection s
    len = Vec2f(point.x, point.z).length();
    if (!isZero(len)) {
        const float theta = 0.5f * scene_rdl2::math::acos(point.x / len) / sPi;
        result.x = point.z >= 0 ? (1.0f - theta) : theta;
    }

    return result;
}
}

ProjectSphericalMap::ProjectSphericalMap(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name)
    : Parent(sceneClass, name)
{
    mSampleFunc = ProjectSphericalMap::sample;
    mSampleFuncv = (scene_rdl2::rdl2::SampleFuncv) ispc::ProjectSphericalMap_getSampleFunc();

    mIspc.mHasValidProjector = false;

    // Store keys to shader data
    mIspc.mRefPKey = moonray::shading::StandardAttributes::sRefP;

    mIspc.mStaticData = (ispc::PROJECTION_StaticData*)&sStaticProjectSphericalMapData;

    // Set projection error messages and fatal color
    projection::initLogEvents(*mIspc.mStaticData, sLogEventRegistry, this);
}

ProjectSphericalMap::~ProjectSphericalMap()
{
}

void
ProjectSphericalMap::update()
{
    mIspc.mHasValidProjector = false;
    mIspc.mXform = nullptr;
    mXform = projection::getProjectorXform(this,
                                           static_cast<ispc::PROJECTION_Mode>(get(attrProjectionMode)),
                                           get(attrProjector),
                                           get(attrProjectionMatrix),
                                           static_cast<ispc::PROJECTION_TransformOrder>(get(attrTRSOrder)),
                                           static_cast<ispc::PROJECTION_RotationOrder>(get(attrRotationOrder)),
                                           get(attrTranslate),
                                           get(attrRotate),
                                           get(attrScale));
    if (mXform != nullptr) {
        mIspc.mXform = mXform->getIspcXform();
        mIspc.mHasValidProjector = true;
    }

    // Use reference space
    if (hasChanged(attrUseReferenceSpace)) {
        mRequiredAttributes.clear();
        mOptionalAttributes.clear();
        if (get(attrUseReferenceSpace)) {
            mRequiredAttributes.push_back(mIspc.mRefPKey);
        }
    }
}

void
ProjectSphericalMap::sample(const scene_rdl2::rdl2::Map* self, moonray::shading::TLState* tls, const moonray::shading::State& state, Color* sample)
{
    const ProjectSphericalMap* me = static_cast<const ProjectSphericalMap*>(self);

    if (!me->mIspc.mHasValidProjector) {
        // Log missing projector data message
        moonray::shading::logEvent(me, me->mIspc.mStaticData->sErrorMissingProjector);
        *sample = Color(0.0f, 0.0f, -1.0f);
        return;
    }

    // Retrieve position in object space
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
                              me->mXform.get(),
                              ispc::SHADING_SPACE_OBJECT,
                              me->mIspc.mRefPKey,
                              pos, pos_ddx, pos_ddy, pos_ddz)) {
        // Log missing ref_P data message
        moonray::shading::logEvent(me, me->mIspc.mStaticData->sErrorMissingRefP);
        *sample = Color(0.0f, 0.0f, -1.0f);
        return;
    }

    // Project position into polar coords
    const Vec3f U = sphereProjection(pos);

    *sample = Color(U.x, U.y, U.z);
}

