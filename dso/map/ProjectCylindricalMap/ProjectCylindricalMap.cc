// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.cc"
#include "ProjectCylindricalMap_ispc_stubs.h"

#include <moonray/map/primvar/Primvar.h>
#include <moonshine/map/projection/ProjectionUtil.h>

#include <moonray/common/mcrt_macros/moonray_static_check.h>
#include <moonray/rendering/shading/MapApi.h>

using namespace moonshine;
using namespace scene_rdl2::math;

static ispc::PROJECTION_StaticData sStaticProjectCylindricalMapData;

//----------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(ProjectCylindricalMap, scene_rdl2::rdl2::Map)

public:
    ProjectCylindricalMap(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name);
    ~ProjectCylindricalMap() override;
    void update() override;

private:
    static void sample(const scene_rdl2::rdl2::Map* self, moonray::shading::TLState* tls, const moonray::shading::State& state, Color* sample);

    ispc::ProjectCylindricalMap mIspc;
    std::unique_ptr<moonray::shading::Xform> mXform;

RDL2_DSO_CLASS_END(ProjectCylindricalMap)

//----------------------------------------------------------------------------

namespace {
Vec3f cylinderProjection(const Vec3f& point)
{
    const float len = Vec2f(point.x, point.z).length();
    if (isZero(len)) return point;

    // Set projection cy
    Vec3f result(0.0f, point.y + 0.5f, 0.0f);

    // Set projection s
    const float theta = 0.5f * scene_rdl2::math::acos(point.x / len) / sPi;
    result.x = point.z >= 0 ? (1.0f - theta) : theta;

    return result;
}
}

ProjectCylindricalMap::ProjectCylindricalMap(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name)
    : Parent(sceneClass, name)
{
    mSampleFunc = ProjectCylindricalMap::sample;
    mSampleFuncv = (scene_rdl2::rdl2::SampleFuncv) ispc::ProjectCylindricalMap_getSampleFunc();

    mIspc.mHasValidProjector = false;

    // Store keys to shader data
    mIspc.mRefPKey = moonray::shading::StandardAttributes::sRefP;
    mIspc.mRefNKey = moonray::shading::StandardAttributes::sRefN;

    mIspc.mStaticData = (ispc::PROJECTION_StaticData*)&sStaticProjectCylindricalMapData;

    // Set projection error messages and fatal color
    projection::initLogEvents(*mIspc.mStaticData, sLogEventRegistry, this);
}

ProjectCylindricalMap::~ProjectCylindricalMap()
{
}

void
ProjectCylindricalMap::update()
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
            if (!get(attrProjectInward) || !get(attrProjectOutward)) {
                mOptionalAttributes.push_back(mIspc.mRefNKey);
            }
        }
    }
}

void
ProjectCylindricalMap::sample(const scene_rdl2::rdl2::Map* self, moonray::shading::TLState* tls, const moonray::shading::State& state, Color* sample)
{
    const ProjectCylindricalMap* me = static_cast<const ProjectCylindricalMap*>(self);

    if (!me->mIspc.mHasValidProjector) {
        // Log missing projector data message
        moonray::shading::logEvent(me, me->mIspc.mStaticData->sErrorMissingProjector);
        *sample = Color(0.0f, 0.0f, -1.0f);
        return;
    }

    // Retrieve position and normal in object space
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

    // check normal, possibly exit early
    if (!me->get(attrProjectInward) || !me->get(attrProjectOutward)) {
        // retrieve normal in object space of projector
        Vec3f normal, inputNormal;
        if (!primvar::getNormal(tls, state,
                                inputSourceMode,
                                inputNormal,
                                me->mXform.get(),
                                ispc::SHADING_SPACE_OBJECT,
                                me->mIspc.mRefPKey,
                                me->mIspc.mRefNKey,
                                normal)) {
            // Log missing ref_N data message
            moonray::shading::logEvent(me, me->mIspc.mStaticData->sErrorMissingRefN);
            *sample = Color(0.0f, 0.0f, -1.0f);
            return;
        }

        // project pos vector onto plane defined by cylinder axis
        const Vec3f pc(pos.x, 0.0f, pos.z);
        const Vec3f nc = state.isEntering() ? normal : -normal;
        const float pDotN = dot(pc, nc);

        if ((!me->get(attrProjectInward) && pDotN < 0.0f) ||
            (!me->get(attrProjectOutward) && pDotN > 0.0f)) {
            // signal invalid tex coord to ImageMap
            *sample = Color(0.f, 0.f, -1.f);
            return;
        }
    }

    // Project position into cylindrical coords
    Vec3f rPos = cylinderProjection(pos);

    // Set blue/w component based on UV region.  If UVs are outside the 0.0 
    // to 1.0 range, the mask is set to 0.0.   If they are inside the range 
    // the mask is set to 1.0.
    if (me->get(attrBlackOutsideProjection)) {
        rPos.z = (rPos[0] < 0.0f || rPos[0] > 1.0f || rPos[1] < 0.0f || rPos[1] > 1.0f) ? -1.0f : 1.0f;
    }
    *sample = Color(rPos.x, rPos.y, rPos.z);
}

