// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.cc"
#include "ProjectPlanarMap_ispc_stubs.h"

#include <moonray/map/primvar/Primvar.h>
#include <moonshine/map/projection/ProjectionUtil.h>

#include <moonray/common/mcrt_macros/moonray_static_check.h>
#include <moonray/rendering/shading/MapApi.h>

using namespace moonshine;
using namespace scene_rdl2::math;

static ispc::PROJECTION_StaticData sStaticProjectPlanarMapData;

//----------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(ProjectPlanarMap, scene_rdl2::rdl2::Map)

public:
    ProjectPlanarMap(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name);
    ~ProjectPlanarMap() override;
    void update() override;

private:
    static void sample(const scene_rdl2::rdl2::Map* self, moonray::shading::TLState* tls, const moonray::shading::State& state, Color* sample);
    ispc::ProjectPlanarMap mIspc;
    std::unique_ptr<moonray::shading::Xform> mXform;

RDL2_DSO_CLASS_END(ProjectPlanarMap)

//----------------------------------------------------------------------------

ProjectPlanarMap::ProjectPlanarMap(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name)
    : Parent(sceneClass, name)
{
    mSampleFunc = ProjectPlanarMap::sample;
    mSampleFuncv = (scene_rdl2::rdl2::SampleFuncv) ispc::ProjectPlanarMap_getSampleFunc();

    mIspc.mHasValidProjector = false;

    // Store keys to shader data
    mIspc.mRefPKey = moonray::shading::StandardAttributes::sRefP;
    mIspc.mRefNKey = moonray::shading::StandardAttributes::sRefN;

    mIspc.mStaticData = (ispc::PROJECTION_StaticData*)&sStaticProjectPlanarMapData;

    // Set projection error messages and fatal color
    projection::initLogEvents(*mIspc.mStaticData, sLogEventRegistry, this);
}

ProjectPlanarMap::~ProjectPlanarMap()
{
}

void
ProjectPlanarMap::update()
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

    // object space to uv space
    const Xform3f o2uv(Vec3f(1.0f, 0.0f, 0.0f),
                       Vec3f(0.0f, 1.0f, 0.0f),
                       Vec3f(0.0f, 0.0f, 1.0f),
                       Vec3f(0.5f, 0.5f, 0.0f));

    asCpp(mIspc.o2uv) = o2uv;

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
ProjectPlanarMap::sample(const scene_rdl2::rdl2::Map* self, moonray::shading::TLState* tls, const moonray::shading::State& state, Color* sample)
{
    const ProjectPlanarMap* me = static_cast<const ProjectPlanarMap*>(self);

    *sample = Color(0.0f, 0.0f, -1.0f);

    if (!me->mIspc.mHasValidProjector) {
        // Log missing projector data message
        moonray::shading::logEvent(me, me->mIspc.mStaticData->sErrorMissingProjector);
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
        return;
    }

    // don't test the normal unless we need to
    bool isBackfacing = false;

    if (!me->get(attrProjectOnBackFaces)) {
        Vec3f projectedN, inputNormal;
        if (!primvar::getNormal(tls, state,
                                inputSourceMode,
                                inputNormal,
                                me->mXform.get(),
                                ispc::SHADING_SPACE_OBJECT,
                                me->mIspc.mRefPKey,
                                me->mIspc.mRefNKey,
                                projectedN)) {
            // Log missing ref_N data message
            moonray::shading::logEvent(me, me->mIspc.mStaticData->sErrorMissingRefN);
            return;
        }

        if (projectedN.z < 0.f) { isBackfacing = true; }
    }

    // Set blue/w component based on UV region and back facing.  If UVs are
    // outside the 0.0 to 1.0 range or the surface is back facing and project
    // on back faces is off, the mask is set to -1.0.   If they are inside
    // the range the mask is set to 1.0.   This value can be used by downstream
    // maps (i.e. ImageMap) to mask these regions.
    Vec3f uvw;
    if (isBackfacing && !me->get(attrProjectOnBackFaces)) {
        // invalid tex coord
        uvw = Vec3f(0.0f, 0.0f, -1.0f);
    } else {
        // Transform screen space position to UV space
        uvw = transformPoint(asCpp(me->mIspc.o2uv), pos);
        if (me->get(attrBlackOutsideProjection)) {
            // valid tex coord ?
            uvw.z = (uvw[0] < 0.0f ||
                     uvw[0] > 1.0f ||
                     uvw[1] < 0.0f ||
                     uvw[1] > 1.0f) ? -1.0f : 1.0f;
        } else {
            // valid tex coord
            uvw.z = 1.f;
        }
    }

    *sample = Color(uvw.x, uvw.y, uvw.z);
}

