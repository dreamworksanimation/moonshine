// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.cc"
#include "ProjectTriplanarUdimMap_ispc_stubs.h"

#include <moonray/map/primvar/Primvar.h>
#include <moonshine/map/projection/ProjectionUtil.h>

#include <moonray/common/mcrt_macros/moonray_static_check.h>
#include <moonray/rendering/shading/MapApi.h>

using namespace moonshine;
using namespace scene_rdl2::math;

static ispc::PROJECTION_StaticData sStaticProjectTriplanarUdimMapData;

//----------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(ProjectTriplanarUdimMap, Map)

public:
ProjectTriplanarUdimMap(const SceneClass& sceneClass, const std::string& name);
    ~ProjectTriplanarUdimMap() override;
    void update() override;

private:
    static void sample(const Map* self, moonray::shading::TLState* tls,
                       const moonray::shading::State& state, Color* sample);

    ispc::ProjectTriplanarUdimMap mIspc;
    std::unique_ptr<moonray::shading::Xform> mXform;

RDL2_DSO_CLASS_END(ProjectTriplanarUdimMap)

//----------------------------------------------------------------------------

namespace {


Xform3f
composeTransform2D( 
    const Vec2f &translation, 
    const Vec2f &scale,
    const float rotation, 
    const Vec2f &rotationCenter
){
    Xform3f result = Xform3f(scene_rdl2::math::one);


    // To Rotation Center
    result = result * Xform3f::translate(
        Vec3f(-rotationCenter.x, -rotationCenter.y, 0));

    // Rotation
    result = result * Xform3f::rotate(
        Vec3f(0.0f, 0.0f, 1.0f), rotation);

    // From Rotation Center
    result = result * Xform3f::translate(
        Vec3f(rotationCenter.x, rotationCenter.y, 0));

    // Scale
    result = result * Xform3f::scale(
        Vec3f(scale.x, scale.y, 0));

    // Translate
    result = result * Xform3f::translate(
        Vec3f(translation.x, translation.y, 0));

    return result;
}

// Maps a given u or v coordinate value to [0, 1) range
// Input value can be positive or negative
inline float
remapToZeroOne(float value)
{
    float result = scene_rdl2::math::fmod(value, 1.0f); // Maps value to [-1, 0) or [0, 1)

    // If above mapping results in [-1, 0), map it to [0, 1)
    return (result < 0) ? result + 1.0f : result;
}

int
chooseAxis(const Vec3f& normal)
{
    int max = 0;
    Vec3f absNormal = scene_rdl2::math::abs(normal);

    // Check which coordinate among x,y,z is greatest
    // to select the axis to project the texture
    if (absNormal.y > absNormal.x) max = 1;
    if (absNormal.z > absNormal[max]) max = 2;

    // Return 0 for X, 1 for Y and 2 for Z axis projection
    return max;
}

Vec3f
computeUv(const Vec3f& pos, const Vec3f& normal,
    int axis, bool useCorrectUVs,
    const Xform3f &xProj2DXform,
    const Xform3f &yProj2DXform,
    const Xform3f &zProj2DXform )
{
    Vec3f resultUv(scene_rdl2::math::zero);

    // Set correct u,v coordinates based on the face that is picked for projection
    // Offset u to account for UDIM layout of the three textures
    // Points projected outside [0, 1) are mapped back into [0, 1) range
    // Add 0.5 to u and v to account for center at the origin
    switch(axis) {
    case 2: // Project Z (XY plane), UDIM: 1003
        resultUv.x = pos.x + 0.5f;
        resultUv.y = pos.y + 0.5f;
        if (useCorrectUVs) {
            resultUv.x = 1.0f - resultUv.x;
        }
        resultUv = transformPoint(zProj2DXform, resultUv);
        resultUv.x = remapToZeroOne(resultUv.x);
        resultUv.y = remapToZeroOne(resultUv.y);

        resultUv.x += 2.0f - sEpsilon; // For UDIM
        break;
    case 1: // Project Y (XZ plane), UDIM 1002
        resultUv.x = pos.x + 0.5f;
        resultUv.y = pos.z + 0.5f;
        if (useCorrectUVs) {
            resultUv.y = 1.0f - resultUv.y;
        }
        resultUv = transformPoint(yProj2DXform, resultUv);
        resultUv.x = remapToZeroOne(resultUv.x);
        resultUv.y = remapToZeroOne(resultUv.y);


        resultUv.x += 1.0f - sEpsilon; // For UDIM
        break;
    case 0: // Project X (YZ plane), UDIM 1001
        resultUv.x = pos.z + 0.5f;
        resultUv.y = pos.y + 0.5f;
        if (useCorrectUVs) {
            resultUv.x = 1.0f - resultUv.x;
        }
        resultUv = transformPoint(xProj2DXform, resultUv);
        resultUv.x = remapToZeroOne(resultUv.x);
        resultUv.y = remapToZeroOne(resultUv.y);

        // No UDIM offset in this case
        break;
    }

    return resultUv;
}
} // namespace

ProjectTriplanarUdimMap::ProjectTriplanarUdimMap(const SceneClass& sceneClass, const std::string& name)
    : Parent(sceneClass, name)
{
    mSampleFunc = ProjectTriplanarUdimMap::sample;
    mSampleFuncv = (SampleFuncv) ispc::ProjectTriplanarUdimMap_getSampleFunc();

    mIspc.mHasValidProjector = false;

    // Store keys to shader data
    mIspc.mRefPKey = moonray::shading::StandardAttributes::sRefP;
    mIspc.mRefNKey = moonray::shading::StandardAttributes::sRefN;

    mIspc.mStaticData = (ispc::PROJECTION_StaticData*)&sStaticProjectTriplanarUdimMapData;

    // Set projection error messages and fatal color
    projection::initLogEvents(*mIspc.mStaticData,
                              mLogEventRegistry,
                              this);
}

ProjectTriplanarUdimMap::~ProjectTriplanarUdimMap()
{
}

void
ProjectTriplanarUdimMap::update()
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
            mOptionalAttributes.push_back(mIspc.mRefNKey);
        }
    }

    // Cache 2D per-axis transforms
    const Xform3f xProj2DXform = composeTransform2D(
        get(attrXOffset), 
        get(attrXScale), 
        deg2rad(get(attrXRotation)), 
        get(attrXRotationCenter)
    );
    asCpp(mIspc.xProj2DXform) = xProj2DXform;

    const Xform3f yProj2DXform = composeTransform2D(
        get(attrYOffset), 
        get(attrYScale), 
        deg2rad(get(attrYRotation)), 
        get(attrYRotationCenter)
    );
    asCpp(mIspc.yProj2DXform) = yProj2DXform;

    const Xform3f zProj2DXform = composeTransform2D(
        get(attrZOffset), 
        get(attrZScale), 
        deg2rad(get(attrZRotation)), 
        get(attrZRotationCenter)
    );
    asCpp(mIspc.zProj2DXform) = zProj2DXform;
}

void
ProjectTriplanarUdimMap::sample(const Map* self, moonray::shading::TLState* tls,
                            const moonray::shading::State& state, Color* sample)
{
    ProjectTriplanarUdimMap const *me = static_cast<ProjectTriplanarUdimMap const *>(self);

    *sample = Color(0.0f, 0.0f, -1.0f);

    if (!me->mIspc.mHasValidProjector) {
        // Log missing projector data message
        moonray::shading::logEvent(me, tls, me->mIspc.mStaticData->sErrorMissingProjector);
        return;
    }

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
                              me->mXform.get(),
                              ispc::SHADING_SPACE_OBJECT,
                              me->mIspc.mRefPKey,
                              pos, pos_ddx, pos_ddy, pos_ddz)) {
        // Log missing ref_P data message
        moonray::shading::logEvent(me, tls, me->mIspc.mStaticData->sErrorMissingRefP);
        return;
    }
 
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
        moonray::shading::logEvent(me, tls, me->mIspc.mStaticData->sErrorMissingRefN);
        return;
    }

    // Choose which texture to project based on normal orientation
    const int axis = chooseAxis(normal);

    const Xform3f &xProj2DXform = asCpp(me->mIspc.xProj2DXform);
    const Xform3f &yProj2DXform = asCpp(me->mIspc.yProj2DXform);
    const Xform3f &zProj2DXform = asCpp(me->mIspc.zProj2DXform);

    const Vec3f uvw = computeUv(pos, normal, axis,
                            me->get(attrUseCorrectUv), xProj2DXform, yProj2DXform,
                            zProj2DXform);

    *sample = Color(uvw.x, uvw.y, 0.f);
}

