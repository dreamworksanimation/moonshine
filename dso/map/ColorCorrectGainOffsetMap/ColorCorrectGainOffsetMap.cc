// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ColorCorrectGainOffsetMap.cc

#include "attributes.cc"
#include "ColorCorrectGainOffsetMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>
#include <moonray/rendering/shading/ColorCorrect.h>

using namespace moonray::shading;
using namespace scene_rdl2::math;
using namespace moonray::pbr;

//----------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(ColorCorrectGainOffsetMap, scene_rdl2::rdl2::Map)

public:
    ColorCorrectGainOffsetMap(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name);
    ~ColorCorrectGainOffsetMap() override;
    void update() override;

private:
    static void sample(const scene_rdl2::rdl2::Map *self, moonray::shading::TLState *tls,
                       const moonray::shading::State &state, Color *sample);

RDL2_DSO_CLASS_END(ColorCorrectGainOffsetMap)

//----------------------------------------------------------------------------

ColorCorrectGainOffsetMap::ColorCorrectGainOffsetMap(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name):
    Parent(sceneClass, name)
{
    mSampleFunc = ColorCorrectGainOffsetMap::sample;
    mSampleFuncv = (scene_rdl2::rdl2::SampleFuncv) ispc::ColorCorrectGainOffsetMap_getSampleFunc();
}

ColorCorrectGainOffsetMap::~ColorCorrectGainOffsetMap()
{
}

void
ColorCorrectGainOffsetMap::update()
{
}

void
ColorCorrectGainOffsetMap::sample(const scene_rdl2::rdl2::Map *self, moonray::shading::TLState *tls,
                        const moonray::shading::State &state, Color *sample)
{
    const ColorCorrectGainOffsetMap* me = static_cast<const ColorCorrectGainOffsetMap*>(self);

    const Color input = evalColor(me, attrInput, tls, state);
    Color result(input);

    const float mix = evalFloat(me, attrMix, tls, state);
    if (!me->get(attrOn) || isZero(mix)) {
        *sample = result;
        return;
    }

    Color gain;
    Color offset;
    if (me->get(attrGainOffsetPerChannel)) {
        gain = Color(evalFloat(me, attrGainR, tls, state),
                     evalFloat(me, attrGainG, tls, state),
                     evalFloat(me, attrGainB, tls, state));
        offset = Color(evalFloat(me, attrOffsetR, tls, state),
                       evalFloat(me, attrOffsetG, tls, state),
                       evalFloat(me, attrOffsetB, tls, state));
    } else {
        gain = Color(evalFloat(me, attrGain, tls, state));
        offset = Color(evalFloat(me, attrOffset, tls, state));
    }

    applyGainAndOffset(gain, offset, result);

    if (!isEqual(mix, 1.0f)) {
        result.r = lerpOpt(input.r, result.r, mix);
        result.g = lerpOpt(input.g, result.g, mix);
        result.b = lerpOpt(input.b, result.b, mix);
    }

    *sample = result;
}

