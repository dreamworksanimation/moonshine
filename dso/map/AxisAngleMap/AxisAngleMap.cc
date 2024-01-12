// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.cc"
#include "AxisAngleMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>

#include <memory>

using namespace scene_rdl2::math;

//----------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(AxisAngleMap, Map)

public:
    AxisAngleMap(const SceneClass& sceneClass, const std::string& name);
    ~AxisAngleMap() override;
    void update() override;

private:
    static void sample(const Map* self, moonray::shading::TLState* tls, const moonray::shading::State& state, Color* sample);
    ispc::AxisAngleMap mIspc;
    std::unique_ptr<moonray::shading::Xform> mXform;

RDL2_DSO_CLASS_END(AxisAngleMap)

//----------------------------------------------------------------------------

AxisAngleMap::AxisAngleMap(const SceneClass& sceneClass, const std::string& name)
    : Parent(sceneClass, name)
{
    mSampleFunc = AxisAngleMap::sample;
    mSampleFuncv = (SampleFuncv) ispc::AxisAngleMap_getSampleFunc();
}

AxisAngleMap::~AxisAngleMap()
{
}

void
AxisAngleMap::update()
{
    mXform = std::make_unique<moonray::shading::Xform>(this);
    mIspc.mXform = mXform->getIspcXform();
}

void
AxisAngleMap::sample(const Map* self, moonray::shading::TLState* tls, const moonray::shading::State& state, Color* sample)
{
    const AxisAngleMap* me = static_cast<const AxisAngleMap*>(self);

    Vec3f inputVector = evalVec3f(me, attrInputVector, tls, state);
    const int inputSpace = me->get(attrInputSpace);
    const int axisSpace = me->get(attrAxisSpace);
    const int outputSpace = me->get(attrOutputSpace);
    const float angle = deg2rad(evalFloat(me, attrAngle, tls, state));

    inputVector = me->mXform->transformNormal(inputSpace,
                                              axisSpace,
                                              state,
                                              inputVector);

    const Vec3f rotationAxis = evalVec3f(me, attrRotationAxis, tls, state);
    const Mat4f mat = Mat4f::rotate(rotationAxis, angle);
    Vec3f outputVector = transformVector(mat, inputVector);

    outputVector = me->mXform->transformNormal(axisSpace,
                                               outputSpace,
                                               state,
                                               outputVector);

    *sample = Color(outputVector.x, outputVector.y, outputVector.z);
}

