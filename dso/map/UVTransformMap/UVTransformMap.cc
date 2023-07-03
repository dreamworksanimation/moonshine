// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file UVTransformMap.cc

/// Offsets, rotates, and scales UV coordinates in any space.

#include "attributes.cc"
#include "UVTransformMap_ispc_stubs.h"

#include <moonray/common/mcrt_macros/moonray_static_check.h>
#include <moonray/rendering/shading/MapApi.h>
#include <scene_rdl2/render/util/stdmemory.h>

using namespace moonray::shading;
using namespace scene_rdl2::math;

static ispc::StaticUVTransformMapData sStaticUVTransformMapData;

RDL2_DSO_CLASS_BEGIN(UVTransformMap, Map)
public:
    UVTransformMap(SceneClass const &sceneClass, std::string const &name);
    ~UVTransformMap() override;
    void update() override;

private:
    static void sample(const Map *self, moonray::shading::TLState *tls,
                       const moonray::shading::State &state, Color *sample);
    void updateRotationXform(float theta, const Vec2f& rotationCenter, const Vec3f& rotationAxis, int space);
    void updateRotationXform2D(float theta, const Vec2f& rotationCenter);
    void updateRotationXform3D(float theta, const Vec3f& rotationAxis);
    void rotateTexCoords(Vec3f& pos) const;

    // These are the members which are visible to both C++ and ISPC.
    ispc::UVTransformMap mIspc;

    std::unique_ptr<moonray::shading::Xform> mXform;

RDL2_DSO_CLASS_END(UVTransformMap)

void
UVTransformMap::updateRotationXform2D(float theta, const Vec2f& rotationCenter)
{
    // 2D rotation is a rotation in the z-axis.
    const Vec3f rotationAxis(0.f, 0.f, 1.f);
    const Vec3f rotationCenter3(rotationCenter.x, rotationCenter.y, 0.f);
    Xform3f xform(scene_rdl2::math::one);
    Xform3f t1(scene_rdl2::math::one);
    Xform3f r(scene_rdl2::math::one);
    Xform3f t2(scene_rdl2::math::one);
    t1 = t1.translate(rotationCenter3);
    r = r.rotate(rotationAxis, theta);
    t2 = t2.translate(-rotationCenter3);
    mIspc.mUVRotationXform = asIspc(t2 * (r * t1));
}

void
UVTransformMap::updateRotationXform3D(float theta, const Vec3f& rotationAxis)
{
    Xform3f xform(scene_rdl2::math::one);
    mIspc.mUVRotationXform = asIspc(xform.rotate(rotationAxis, theta));
}

void
UVTransformMap::updateRotationXform(float theta, const Vec2f& rotationCenter, const Vec3f& rotationAxis, int space)
{
    // Use 3D rotation in 3D spaces.
    if (space == ispc::SHADING_SPACE_WORLD || space == ispc::SHADING_SPACE_OBJECT || space == ispc::SHADING_SPACE_REFERENCE) {
        updateRotationXform3D(theta, rotationAxis);
    } else {
        updateRotationXform2D(theta, rotationCenter);
    }
}

void
UVTransformMap::rotateTexCoords(Vec3f& pos) const
{
    pos = transformPoint(asCpp(mIspc.mUVRotationXform), pos);
}

UVTransformMap::UVTransformMap(const SceneClass& sceneClass,
        const std::string& name) :
    Parent(sceneClass, name)
{
    mSampleFunc = UVTransformMap::sample;
    mSampleFuncv = (SampleFuncv) ispc::UVTransformMap_getSampleFunc();

    // Store keys to shader data
    mIspc.mRefPKey = moonray::shading::StandardAttributes::sRefP;

    mIspc.mUVTransformMapDataPtr = (ispc::StaticUVTransformMapData*)&sStaticUVTransformMapData;

     // register shade time event messages.  we require and expect
    // these events to have the same value across all instances of the shader.
    // no conditional registration of events is allowed.
    // to allow for the possibility that we may someday create image maps
    // on multiple threads, we'll protect the writes of the class statics
    // with a mutex.
    static std::mutex errorMutex;
    std::scoped_lock lock(errorMutex);
    MOONRAY_START_THREADSAFE_STATIC_WRITE
    sStaticUVTransformMapData.sErrorMissingReferenceData =
        mLogEventRegistry.createEvent(scene_rdl2::logging::ERROR_LEVEL,
                                      "missing reference data");
    MOONRAY_FINISH_THREADSAFE_STATIC_WRITE
}

UVTransformMap::~UVTransformMap()
{
}

void
UVTransformMap::update()
{
    // Set fatal color
    const SceneVariables &sv = getSceneClass().getSceneContext()->getSceneVariables();
    mIspc.mFatalColor = asIspc(sv.get(SceneVariables::sFatalColor));

    if (hasChanged(attrSpace)) {
        mRequiredAttributes.clear();
        mOptionalAttributes.clear();
        if (get(attrSpace) == ispc::SHADING_SPACE_REFERENCE) {
            mRequiredAttributes.push_back(mIspc.mRefPKey);
        }
    }

    // Construct Xform with default transforms for camera and screen and geometry.
    mXform = fauxstd::make_unique<moonray::shading::Xform>(this);
    mIspc.mXform = mXform->getIspcXform();

    float theta = deg2rad(get(attrRotationAngle));
    if (!isZero(theta)) {
        updateRotationXform(theta, get(attrRotationCenter), get(attrRotationAxis), get(attrSpace));
    }
}

void
UVTransformMap::sample(const Map* self,
                       moonray::shading::TLState *tls,
                       const moonray::shading::State& state,
                       Color* sample)
{
    const UVTransformMap* me = static_cast<const UVTransformMap*>(self);
    const Vec2f offset = me->get(attrOffset);
    const Vec2f scale  = me->get(attrScale);

    Vec3f pos;
    const int space = me->get(attrSpace);
    if (space == ispc::SHADING_SPACE_TEXTURE) {
        pos = Vec3f(
            state.getSt().x,
            state.getSt().y,
            0.0f
        );
    } else if (space == ispc::SHADING_SPACE_REFERENCE) {
        state.getRefP(pos);
    } else {
        pos = state.getP();
        if (space != ispc::SHADING_SPACE_RENDER) {
            pos = me->mXform->transformPoint(ispc::SHADING_SPACE_RENDER,
                                             space,
                                             state,
                                             pos);
        }
    }

    // Rotate coordinates.
    if (!isZero(me->get(attrRotationAngle))) {
        me->rotateTexCoords(pos);
    }
    // Scale and translate coords.
     pos.x = scale.x * pos.x + offset.x;
     pos.y = scale.y * pos.y + offset.y;

    *sample = Color(pos[0], pos[1], 0.f);
}

