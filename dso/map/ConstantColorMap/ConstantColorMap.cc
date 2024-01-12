// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.cc"
#include "ConstantColorMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>

using namespace scene_rdl2::math;

RDL2_DSO_CLASS_BEGIN(ConstantColorMap, Map)

public:
    ConstantColorMap(const SceneClass& sceneClass, const std::string& name);
    ~ConstantColorMap();
    virtual void update();

private:
    static void sample(const Map* self, moonray::shading::TLState *tls,
                       const moonray::shading::State& state, Color* sample);

RDL2_DSO_CLASS_END(ConstantColorMap)

ConstantColorMap::ConstantColorMap(const SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mSampleFunc = ConstantColorMap::sample;
    mSampleFuncv = (SampleFuncv) ispc::ConstantColorMap_getSampleFunc();
}

ConstantColorMap::~ConstantColorMap()
{
}

void
ConstantColorMap::update()
{
}

void
ConstantColorMap::sample(const Map* self, moonray::shading::TLState *tls,
                 const moonray::shading::State& state, Color* sample)
{
    const ConstantColorMap* me = static_cast<const ConstantColorMap*>(self);
    const Color result = me->get(attrColorValue);
    *sample = result;
}

