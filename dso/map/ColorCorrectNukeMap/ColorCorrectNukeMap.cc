// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ColorCorrectNukeMap.cc

#include "attributes.cc"
#include "ColorCorrectNukeMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>
#include <moonray/rendering/shading/ColorCorrect.h>

using namespace moonray::shading;
using namespace scene_rdl2::math;
using namespace moonray::pbr;

//----------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(ColorCorrectNukeMap, scene_rdl2::rdl2::Map)

public:
    ColorCorrectNukeMap(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name);
    ~ColorCorrectNukeMap() override;
    void update() override;

private:
    static void sample(const scene_rdl2::rdl2::Map *self, moonray::shading::TLState *tls,
                       const moonray::shading::State &state, Color *sample);

RDL2_DSO_CLASS_END(ColorCorrectNukeMap)

//----------------------------------------------------------------------------

ColorCorrectNukeMap::ColorCorrectNukeMap(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name):
    Parent(sceneClass, name)
{
    mSampleFunc = ColorCorrectNukeMap::sample;
    mSampleFuncv = (scene_rdl2::rdl2::SampleFuncv) ispc::ColorCorrectNukeMap_getSampleFunc();
}

ColorCorrectNukeMap::~ColorCorrectNukeMap()
{
}

void
ColorCorrectNukeMap::update()
{
}

void
ColorCorrectNukeMap::sample(const scene_rdl2::rdl2::Map *self, moonray::shading::TLState *tls,
                        const moonray::shading::State &state, Color *sample)
{
    const ColorCorrectNukeMap* me = static_cast<const ColorCorrectNukeMap*>(self);

    const Color input = evalColor(me, attrInput, tls, state);
    Color result(input);

    if (me->get(attrSaturationEnabled)) {
        const Color saturation = me->get(attrSaturation);
        applySaturation(saturation, result);
    }

    if (me->get(attrContrastEnabled)) {
        const Color contrast = me->get(attrContrast);
        applyNukeContrast(contrast, result);
    }

    if (me->get(attrGammaEnabled)) {
        const Color gamma = me->get(attrGamma);
        const Color invGamma = Color(1.0f / max(sEpsilon, gamma.r),
                                     1.0f / max(sEpsilon, gamma.g),
                                     1.0f / max(sEpsilon, gamma.b));
        applyGamma(invGamma, result);
    }

    if (me->get(attrGainOffsetEnabled)) {
        const Color gain = me->get(attrGain);
        const Color offset = me->get(attrOffset);
        applyGainAndOffset(gain, offset, result);
    }

    if (me->get(attrTMIControlEnabled)) {
        // The TMI parameter is kept as a Vec3f for backward 
        // compatibility so we need to convert it to a color
        // to pass to the applyTMI function
        const Vec3f tmiVec = me->get(attrTMI);
        const Color tmi(tmiVec.x, tmiVec.y, tmiVec.z);
        applyTMI(tmi, result);
    }

    *sample = result;
}

