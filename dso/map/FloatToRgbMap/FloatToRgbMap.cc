// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file FloatToRgbMap.cc

/// Combine 3 floats into an RGB Color

#include "attributes.cc"
#include "FloatToRgbMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>

using namespace scene_rdl2::math;

RDL2_DSO_CLASS_BEGIN(FloatToRgbMap, Map)
public:
    FloatToRgbMap(SceneClass const &sceneClass, std::string const &name);
    void update();

private:
    static void sample(const Map *self, moonray::shading::TLState *tls,
                       const moonray::shading::State &state, Color *sample);

RDL2_DSO_CLASS_END(FloatToRgbMap)

FloatToRgbMap::FloatToRgbMap(const SceneClass& sceneClass,
        const std::string& name) :
    Parent(sceneClass, name)
{
    mSampleFunc = FloatToRgbMap::sample;
    mSampleFuncv = (SampleFuncv) ispc::FloatToRgbMap_getSampleFunc();
}

void
FloatToRgbMap::update()
{
}

void
FloatToRgbMap::sample(const Map* self, moonray::shading::TLState *tls,
                 const moonray::shading::State& state, Color* sample)
{
    const FloatToRgbMap* me = static_cast<const FloatToRgbMap*>(self);
    sample->r = evalFloat(me, attrR, tls, state);
    sample->g = evalFloat(me, attrG, tls, state);
    sample->b = evalFloat(me, attrB, tls, state);
    return;
}

