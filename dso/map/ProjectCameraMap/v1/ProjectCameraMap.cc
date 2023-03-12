// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ProjectCameraMap.cc

#include "attributes.cc"
#include "ProjectCameraMap_ispc_stubs.h"

#include <moonray/map/primvar/Primvar.h>
#include <moonshine/map/projection/ProjectionUtil.h>

#include <moonray/common/mcrt_macros/moonray_static_check.h>
#include <moonray/rendering/shading/MapApi.h>
#include <scene_rdl2/render/util/stdmemory.h>

using namespace moonshine;
using namespace scene_rdl2::math;

static ispc::PROJECTION_StaticData sStaticProjectCameraMapData;

//----------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(ProjectCameraMap, scene_rdl2::rdl2::Map)

public:
    ProjectCameraMap(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name);
    ~ProjectCameraMap() override;
    void update() override;

private:
    static void sample(const scene_rdl2::rdl2::Map *self, moonray::shading::TLState *tls,
                       const moonray::shading::State &state, Color *sample);

    ispc::ProjectCameraMap mIspc;
    std::unique_ptr<moonray::shading::Xform> mXform;

RDL2_DSO_CLASS_END(ProjectCameraMap)

//----------------------------------------------------------------------------

ProjectCameraMap::ProjectCameraMap(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name):
    Parent(sceneClass, name)
{
    mSampleFunc = ProjectCameraMap::sample;
    mSampleFuncv = (scene_rdl2::rdl2::SampleFuncv) ispc::ProjectCameraMap_getSampleFunc();

    mIspc.mHasValidProjector = false;

    // Store keys to shader data
    mIspc.mRefPKey = moonray::shading::StandardAttributes::sRefP;
    mIspc.mRefNKey = moonray::shading::StandardAttributes::sRefN;

    mIspc.mStaticData = (ispc::PROJECTION_StaticData*)&sStaticProjectCameraMapData;

    // Set projection error messages and fatal color
    projection::initLogEvents(*mIspc.mStaticData,
                              mLogEventRegistry,
                              this);
}

ProjectCameraMap::~ProjectCameraMap()
{
}

void
ProjectCameraMap::update()
{
    // Construct Xform object with custom camera and custom square window
    mIspc.mHasValidProjector = false;
    const scene_rdl2::rdl2::SceneObject* projectorObject = get(attrProjector);
    if (projectorObject != nullptr) {
        const scene_rdl2::rdl2::Camera* projectorCamera = projectorObject->asA<scene_rdl2::rdl2::Camera>();
        if (projectorCamera != nullptr) {
            std::array<float, 4> window = { -1.0f, -1.0f, 1.0f, 1.0f };
            if (get(attrUseCustomWindowCoordinates)) {
                window[0] = get(attrWindowXMin);
                window[1] = get(attrWindowYMin);
                window[2] = get(attrWindowXMax);
                window[3] = get(attrWindowYMax);
            }
            mXform = fauxstd::make_unique<moonray::shading::Xform>(this, nullptr, projectorCamera, &window);
            mIspc.mXform = mXform->getIspcXform();
            mIspc.mHasValidProjector = true;
        } else {
            return;
        }
    } else {
        return;
    }

    // map [-1, 1] -> [0, 1]
    const Xform3f s2uv(Vec3f(0.5, 0.0, 0.0),
                       Vec3f(0.0, 0.5, 0.0),
                       Vec3f(0.0, 0.0, 1.0),
                       Vec3f(0.5, 0.5, 0.0));

    asCpp(mIspc.s2uv) = s2uv;

    // Use reference space
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
ProjectCameraMap::sample(const scene_rdl2::rdl2::Map *self, moonray::shading::TLState *tls,
                         const moonray::shading::State &state, Color *sample)
{
    ProjectCameraMap const *me = static_cast<ProjectCameraMap const *>(self);

    *sample = Color(0.0f, 0.0f, -1.0f);

    if (!me->mIspc.mHasValidProjector) {
        // Log missing projector data message
        moonray::shading::logEvent(me, tls, me->mIspc.mStaticData->sErrorMissingProjector);
        return;
    }

    // Retrieve position in screen space and normal in projection camera's space
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
                              ispc::SHADING_SPACE_SCREEN,
                              me->mIspc.mRefPKey,
                              pos, pos_ddx, pos_ddy, pos_ddz)) {
        // Log missing ref_P data message
        moonray::shading::logEvent(me, tls, me->mIspc.mStaticData->sErrorMissingRefP);
        return;
    }


    Vec3f cameraN(0.f, 0.f, -1.f); // default to back facing
    if (!me->get(attrProjectOnBackFaces)) {
        Vec3f inputNormal;
        if (!primvar::getNormal(tls, state,
                                inputSourceMode,
                                inputNormal,
                                me->mXform.get(),
                                ispc::SHADING_SPACE_CAMERA,
                                me->mIspc.mRefPKey,
                                me->mIspc.mRefNKey,
                                cameraN)) {
            // Log missing ref_N data message
            moonray::shading::logEvent(me, tls, me->mIspc.mStaticData->sErrorMissingRefN);
            return;
        }
    }

    // Set blue/w component based on UV region and back facing.  If UVs are
    // outside the 0.0 to 1.0 range or the surface is back facing and project
    // on back faces is off, the mask is set to -1.0.   If they are inside
    // the range the mask is set to 1.0.   This value can be used by downstream
    // maps (i.e. ImageMap) to mask these regions.
    Vec3f uvw;
    const bool frontFacing = cameraN.z > 0.0f;
    if (me->get(attrProjectOnBackFaces) || frontFacing) {
        // Transform screen space position to UV space
        uvw = transformPoint(asCpp(me->mIspc.s2uv), pos);
        if (me->get(attrBlackOutsideProjection)) {
            uvw.z = (uvw[0] < 0.0f || uvw[0] > 1.0f || uvw[1] < 0.0f || uvw[1] > 1.0f) ? -1.0f : 1.0f;
        } else {
            // set w coord to 1 to let ImageMap know this coord is valid
            uvw.z = 1.0f;
        }
    } else {
        uvw = Vec3f(0.0f, 0.0f, -1.0f);
    }

    *sample = Color(uvw.x, uvw.y, uvw.z);
}

