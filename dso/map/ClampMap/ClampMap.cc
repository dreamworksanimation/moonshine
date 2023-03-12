// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ClampMap.cc

#include "attributes.cc"
#include "ClampMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>

using namespace moonray::shading;
using namespace scene_rdl2::math;

//---------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(ClampMap, scene_rdl2::rdl2::Map)

public:
    ClampMap(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name);
    ~ClampMap();
    virtual void update();

private:
    static void sample(const scene_rdl2::rdl2::Map* self, moonray::shading::TLState *tls,
                       const moonray::shading::State& state, Color* sample);

RDL2_DSO_CLASS_END(ClampMap)

//---------------------------------------------------------------------------

ClampMap::ClampMap(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mSampleFunc = ClampMap::sample;
    mSampleFuncv = (scene_rdl2::rdl2::SampleFuncv) ispc::ClampMap_getSampleFunc();
}

ClampMap::~ClampMap()
{
}

void
ClampMap::update()
{
}

void
ClampMap::sample(const scene_rdl2::rdl2::Map* self, moonray::shading::TLState *tls,
                 const moonray::shading::State& state, Color* sample)
{
    const ClampMap* me = static_cast<const ClampMap*>(self);
    Color result = evalColor(me, attrInput, tls, state);

    if (me->get(attrClamp)) {
        for (unsigned int i = 0; i < 3; ++i) {
            result[i] = clamp(result[i], me->get(attrClampMin), me->get(attrClampMax));
        }
    }

    *sample = result;
}


//---------------------------------------------------------------------------

