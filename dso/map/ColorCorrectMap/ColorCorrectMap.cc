// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ColorCorrectMap.cc

#include "attributes.cc"
#include "ColorCorrectMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>
#include <moonray/rendering/shading/ColorCorrect.h>

using namespace moonray::shading;
using namespace scene_rdl2::math;
using namespace moonray::pbr;

//----------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(ColorCorrectMap, scene_rdl2::rdl2::Map)

public:
    ColorCorrectMap(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name);
    ~ColorCorrectMap() override;
    void update() override;

private:
    static void sample(const scene_rdl2::rdl2::Map *self, moonray::shading::TLState *tls,
                       const moonray::shading::State &state, Color *sample);

RDL2_DSO_CLASS_END(ColorCorrectMap)

//----------------------------------------------------------------------------

ColorCorrectMap::ColorCorrectMap(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name):
    Parent(sceneClass, name)
{
    mSampleFunc = ColorCorrectMap::sample;
    mSampleFuncv = (scene_rdl2::rdl2::SampleFuncv) ispc::ColorCorrectMap_getSampleFunc();
}

ColorCorrectMap::~ColorCorrectMap()
{
}

void
ColorCorrectMap::update()
{
}

void
ColorCorrectMap::sample(const scene_rdl2::rdl2::Map *self, moonray::shading::TLState *tls,
                        const moonray::shading::State &state, Color *sample)
{
    const ColorCorrectMap* me = static_cast<const ColorCorrectMap*>(self);

    const Color input = evalColor(me, attrInput, tls, state);
    Color result(input);

    const float mix = evalFloat(me, attrMix, tls, state);
    if (!me->get(attrOn) || isZero(mix)) {
        *sample = result;
        return;
    }

    if (me->get(attrHueShiftEnabled)) {
        const float hueShift = evalFloat(me, attrHueShift, tls, state);
        applyHueShift(hueShift, result);
    }

    if (me->get(attrSaturationEnabled)) {
        Color saturation;
        if (me->get(attrSaturationPerChannel)) {
            saturation = Color(evalFloat(me, attrSaturationR, tls, state),
                               evalFloat(me, attrSaturationG, tls, state),
                               evalFloat(me, attrSaturationB, tls, state));
        } else {
            saturation = Color(evalFloat(me, attrSaturation, tls, state));
        }
        applySaturation(saturation, result);
    }

    if (me->get(attrContrastEnabled)) {
        Color contrast;
        if (me->get(attrContrastPerChannel)) {
            contrast = Color(evalFloat(me, attrContrastR, tls, state),
                             evalFloat(me, attrContrastG, tls, state),
                             evalFloat(me, attrContrastB, tls, state));
        } else {
            contrast = Color(evalFloat(me, attrContrast, tls, state));
        }
        applyContrast(contrast, result);
    }

    if (me->get(attrGammaEnabled)) {
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
    }

    if (me->get(attrGainOffsetEnabled)) {
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
    }

    if (me->get(attrTMIControlEnabled)) {
        const Color tmi = evalColor(me, attrTMI, tls, state);
        applyTMI(tmi, result);
    }

    if (!isEqual(mix, 1.0f)) {
        result.r = lerpOpt(input.r, result.r, mix);
        result.g = lerpOpt(input.g, result.g, mix);
        result.b = lerpOpt(input.b, result.b, mix);
    }

    if (me->get(attrClamp)) {
        for (unsigned int i = 0; i < 3; ++i) {
            result[i] = clamp(result[i], me->get(attrClampMin), me->get(attrClampMax));
        }
    }

    *sample = result;
}

