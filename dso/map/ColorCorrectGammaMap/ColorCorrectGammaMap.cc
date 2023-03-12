// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ColorCorrectGammaMap.cc

#include "attributes.cc"
#include "ColorCorrectGammaMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>
#include <moonray/rendering/shading/ColorCorrect.h>

using namespace moonray::shading;
using namespace scene_rdl2::math;
using namespace moonray::pbr;

//----------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(ColorCorrectGammaMap, scene_rdl2::rdl2::Map)

public:
    ColorCorrectGammaMap(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name);
    ~ColorCorrectGammaMap() override;
    void update() override;

private:
    static void sample(const scene_rdl2::rdl2::Map *self, moonray::shading::TLState *tls,
                       const moonray::shading::State &state, Color *sample);

RDL2_DSO_CLASS_END(ColorCorrectGammaMap)

//----------------------------------------------------------------------------

ColorCorrectGammaMap::ColorCorrectGammaMap(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name):
    Parent(sceneClass, name)
{
    mSampleFunc = ColorCorrectGammaMap::sample;
    mSampleFuncv = (scene_rdl2::rdl2::SampleFuncv) ispc::ColorCorrectGammaMap_getSampleFunc();
}

ColorCorrectGammaMap::~ColorCorrectGammaMap()
{
}

void
ColorCorrectGammaMap::update()
{
}

void
ColorCorrectGammaMap::sample(const scene_rdl2::rdl2::Map *self, moonray::shading::TLState *tls,
                        const moonray::shading::State &state, Color *sample)
{
    const ColorCorrectGammaMap* me = static_cast<const ColorCorrectGammaMap*>(self);

    const Color input = evalColor(me, attrInput, tls, state);
    Color result(input);

    const float mix = evalFloat(me, attrMix, tls, state);
    if (!me->get(attrOn) || isZero(mix)) {
        *sample = result;
        return;
    }

    Color gamma;
    if (me->get(attrGammaPerChannel)) {
        gamma = Color(evalFloat(me, attrGammaR, tls, state),
                      evalFloat(me, attrGammaG, tls, state),
                      evalFloat(me, attrGammaB, tls, state));
    } else {
        gamma = Color(evalFloat(me, attrGamma, tls, state));
    }
    const Color invGamma = Color(1.0f / max(sEpsilon, gamma.r),
                                 1.0f / max(sEpsilon, gamma.g),
                                 1.0f / max(sEpsilon, gamma.b));
    applyGamma(invGamma, result);

    if (!isEqual(mix, 1.0f)) {
        result.r = lerpOpt(input.r, result.r, mix);
        result.g = lerpOpt(input.g, result.g, mix);
        result.b = lerpOpt(input.b, result.b, mix);
    }

    *sample = result;
}

