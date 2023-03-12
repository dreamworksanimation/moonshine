// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.cc"
#include "ConstantScalarMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>

using namespace scene_rdl2::math;

RDL2_DSO_CLASS_BEGIN(ConstantScalarMap, Map)

public:
    ConstantScalarMap(const SceneClass& sceneClass, const std::string& name);
    ~ConstantScalarMap();
    virtual void update();

private:
    static void sample(const Map* self, moonray::shading::TLState *tls,
                       const moonray::shading::State& state, Color* sample);

RDL2_DSO_CLASS_END(ConstantScalarMap)

ConstantScalarMap::ConstantScalarMap(const SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mSampleFunc = ConstantScalarMap::sample;
    mSampleFuncv = (SampleFuncv) ispc::ConstantScalarMap_getSampleFunc();
}

ConstantScalarMap::~ConstantScalarMap()
{
}

void
ConstantScalarMap::update()
{
}

void
ConstantScalarMap::sample(const Map* self, moonray::shading::TLState *tls,
                 const moonray::shading::State& state, Color* sample)
{
    const ConstantScalarMap* me = static_cast<const ConstantScalarMap*>(self);
    const float scalar = me->get(attrScalarValue);
    Color result(scalar);
    *sample = result;
}

