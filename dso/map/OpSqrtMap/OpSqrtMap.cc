// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.cc"
#include "OpSqrtMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>

#include <random>
#include <string>
#include <vector>
#include <math.h>

using namespace scene_rdl2::math;
using namespace moonray::shading;

//---------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(OpSqrtMap, scene_rdl2::rdl2::Map)

public:
    OpSqrtMap(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name);
    ~OpSqrtMap();
    virtual void update();

private:
    static void sample(const scene_rdl2::rdl2::Map* self, moonray::shading::TLState *tls,
                       const moonray::shading::State& state, Color* sample);

RDL2_DSO_CLASS_END(OpSqrtMap)

//---------------------------------------------------------------------------

OpSqrtMap::OpSqrtMap(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mSampleFunc = OpSqrtMap::sample;
    mSampleFuncv = (scene_rdl2::rdl2::SampleFuncv) ispc::OpSqrtMap_getSampleFunc();
}

OpSqrtMap::~OpSqrtMap()
{
}

void
OpSqrtMap::update()
{
}

void
OpSqrtMap::sample(const scene_rdl2::rdl2::Map* self, moonray::shading::TLState *tls,
                 const moonray::shading::State& state, Color* sample)
{
    const OpSqrtMap* me = static_cast<const OpSqrtMap*>(self);

    // Get the input parameter
    const Color input       = evalColor(me, attrInput, tls, state);

    // Do the sqrt
    Color result(input);

    for (size_t i = 0; i < 3; ++i) {
        float x = input[i];
        x = scene_rdl2::math::sqrt(x);
        result[i] = x;
    }


    *sample = result;
}


//---------------------------------------------------------------------------

