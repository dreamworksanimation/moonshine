// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ColorCorrectTMIMap.cc

#include "attributes.cc"
#include "ColorCorrectTMIMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>
#include <moonray/rendering/shading/ColorCorrect.h>

using namespace moonray::shading;
using namespace scene_rdl2::math;
using namespace moonray::pbr;

//----------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(ColorCorrectTMIMap, scene_rdl2::rdl2::Map)

public:
    ColorCorrectTMIMap(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name);
    ~ColorCorrectTMIMap() override;
    void update() override;

private:
    static void sample(const scene_rdl2::rdl2::Map *self, moonray::shading::TLState *tls,
                       const moonray::shading::State &state, Color *sample);

RDL2_DSO_CLASS_END(ColorCorrectTMIMap)

//----------------------------------------------------------------------------

ColorCorrectTMIMap::ColorCorrectTMIMap(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name):
    Parent(sceneClass, name)
{
    mSampleFunc = ColorCorrectTMIMap::sample;
    mSampleFuncv = (scene_rdl2::rdl2::SampleFuncv) ispc::ColorCorrectTMIMap_getSampleFunc();
}

ColorCorrectTMIMap::~ColorCorrectTMIMap()
{
}

void
ColorCorrectTMIMap::update()
{
}

void
ColorCorrectTMIMap::sample(const scene_rdl2::rdl2::Map *self, moonray::shading::TLState *tls,
                        const moonray::shading::State &state, Color *sample)
{
    const ColorCorrectTMIMap* me = static_cast<const ColorCorrectTMIMap*>(self);

    const Color input = evalColor(me, attrInput, tls, state);
    Color result(input);

    const float mix = evalFloat(me, attrMix, tls, state);
    if (!me->get(attrOn) || isZero(mix)) {
        *sample = result;
        return;
    }

    const Color tmi = evalColor(me, attrTMI, tls, state);
    applyTMI(tmi, result);

    if (!isEqual(mix, 1.0f)) {
        result.r = lerpOpt(input.r, result.r, mix);
        result.g = lerpOpt(input.g, result.g, mix);
        result.b = lerpOpt(input.b, result.b, mix);
    }

    *sample = result;
}

