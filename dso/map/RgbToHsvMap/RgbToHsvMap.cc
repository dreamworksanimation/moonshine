// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file RgbToHsvMap.cc

#include "attributes.cc"
#include "RgbToHsvMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>
#include <scene_rdl2/common/math/ColorSpace.h>


using namespace moonray::shading;
using namespace scene_rdl2::math;
using namespace scene_rdl2::rdl2;

//----------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(RgbToHsvMap, Map)

public:
    RgbToHsvMap(const SceneClass &sceneClass, const std::string &name);
    ~RgbToHsvMap() override;
    void update() override;

private:
    static void sample(const Map *self, moonray::shading::TLState *tls,
                       const moonray::shading::State &state, Color *sample);

RDL2_DSO_CLASS_END(RgbToHsvMap)

//----------------------------------------------------------------------------

RgbToHsvMap::RgbToHsvMap(const SceneClass &sceneClass, const std::string &name):
    Parent(sceneClass, name)
{
    mSampleFunc = RgbToHsvMap::sample;
    mSampleFuncv = (SampleFuncv) ispc::RgbToHsvMap_getSampleFunc();
}

RgbToHsvMap::~RgbToHsvMap()
{
}

void
RgbToHsvMap::update()
{
}

void
RgbToHsvMap::sample(const Map *self, moonray::shading::TLState *tls,
                        const moonray::shading::State &state, Color *sample)
{
    const RgbToHsvMap* me = static_cast<const RgbToHsvMap*>(self);
    const Color input = evalColor(me, attrInput, tls, state);
    Color result = rgbToHsv(input);
    *sample = result;
}

