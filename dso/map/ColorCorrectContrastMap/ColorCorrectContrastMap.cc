// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ColorCorrectContrastMap.cc

#include "attributes.cc"
#include "ColorCorrectContrastMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>
#include <moonray/rendering/shading/ColorCorrect.h>

using namespace moonray::shading;
using namespace scene_rdl2::math;
using namespace moonray::pbr;

//----------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(ColorCorrectContrastMap, scene_rdl2::rdl2::Map)

public:
    ColorCorrectContrastMap(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name);
    ~ColorCorrectContrastMap() override;
    void update() override;

private:
    static void sample(const scene_rdl2::rdl2::Map *self, moonray::shading::TLState *tls,
                       const moonray::shading::State &state, Color *sample);

RDL2_DSO_CLASS_END(ColorCorrectContrastMap)

//----------------------------------------------------------------------------

ColorCorrectContrastMap::ColorCorrectContrastMap(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name):
    Parent(sceneClass, name)
{
    mSampleFunc = ColorCorrectContrastMap::sample;
    mSampleFuncv = (scene_rdl2::rdl2::SampleFuncv) ispc::ColorCorrectContrastMap_getSampleFunc();
}

ColorCorrectContrastMap::~ColorCorrectContrastMap()
{
}

void
ColorCorrectContrastMap::update()
{
}

void
ColorCorrectContrastMap::sample(const scene_rdl2::rdl2::Map *self, moonray::shading::TLState *tls,
                        const moonray::shading::State &state, Color *sample)
{
    const ColorCorrectContrastMap* me = static_cast<const ColorCorrectContrastMap*>(self);

    const Color input = evalColor(me, attrInput, tls, state);
    Color result(input);

    const float mix = evalFloat(me, attrMix, tls, state);
    if (!me->get(attrOn) || isZero(mix)) {
        *sample = result;
        return;
    }

    Color contrast;
    if (me->get(attrContrastPerChannel)) {
        contrast = Color(evalFloat(me, attrContrastR, tls, state),
                         evalFloat(me, attrContrastG, tls, state),
                         evalFloat(me, attrContrastB, tls, state));
    } else {
        contrast = Color(evalFloat(me, attrContrast, tls, state));
    }
    applyContrast(contrast, result);

    if (!isEqual(mix, 1.0f)) {
        result.r = lerpOpt(input.r, result.r, mix);
        result.g = lerpOpt(input.g, result.g, mix);
        result.b = lerpOpt(input.b, result.b, mix);
    }

    *sample = result;
}

