// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file HsvToRgbMap.cc

#include "attributes.cc"
#include "HsvToRgbMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>
#include <scene_rdl2/common/math/ColorSpace.h>

using namespace moonray::shading;
using namespace scene_rdl2::math;

//----------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(HsvToRgbMap, scene_rdl2::rdl2::Map)

public:
    HsvToRgbMap(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name);
    ~HsvToRgbMap() override;
    void update() override;

private:
    static void sample(const scene_rdl2::rdl2::Map *self, moonray::shading::TLState *tls,
                       const moonray::shading::State &state, Color *sample);

RDL2_DSO_CLASS_END(HsvToRgbMap)

//----------------------------------------------------------------------------

HsvToRgbMap::HsvToRgbMap(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name):
    Parent(sceneClass, name)
{
    mSampleFunc = HsvToRgbMap::sample;
    mSampleFuncv = (scene_rdl2::rdl2::SampleFuncv) ispc::HsvToRgbMap_getSampleFunc();
}

HsvToRgbMap::~HsvToRgbMap()
{
}

void
HsvToRgbMap::update()
{
}

void
HsvToRgbMap::sample(const scene_rdl2::rdl2::Map *self, moonray::shading::TLState *tls,
                        const moonray::shading::State &state, Color *sample)
{
    const HsvToRgbMap* me = static_cast<const HsvToRgbMap*>(self);
    const Color input = evalColor(me, attrInput, tls, state);
    Color result = hsvToRgb(input);
    *sample = result;
}

