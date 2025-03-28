// Copyright 2025 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.cc"
#include "TwoSidedMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>

using namespace scene_rdl2::math;

RDL2_DSO_CLASS_BEGIN(TwoSidedMap, Map)

public:
    TwoSidedMap(const SceneClass& sceneClass, const std::string& name);
    virtual void update();

private:
    static void sample(const Map* self,
                       moonray::shading::TLState *tls,
                       const moonray::shading::State& state,
                       Color* sample);

RDL2_DSO_CLASS_END(TwoSidedMap)

TwoSidedMap::TwoSidedMap(const SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mSampleFunc = TwoSidedMap::sample;
    mSampleFuncv = (SampleFuncv) ispc::TwoSidedMap_getSampleFunc();
}

void
TwoSidedMap::update()
{
}

void
TwoSidedMap::sample(const Map* self,
                    moonray::shading::TLState *tls,
                    const moonray::shading::State& state,
                    Color* sample)
{
    const TwoSidedMap* me = static_cast<const TwoSidedMap*>(self);
    if (state.isEntering()) {
        *sample = evalColor(me, attrFrontColor, tls, state);
    } else {
        *sample = evalColor(me, attrBackColor, tls, state);
    }
}

