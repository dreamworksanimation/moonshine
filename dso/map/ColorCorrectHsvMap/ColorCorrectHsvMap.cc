// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ColorCorrectHsvMap.cc

#include "attributes.cc"
#include "ColorCorrectHsvMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>
#include <scene_rdl2/common/math/ColorSpace.h>

using namespace moonray::shading;
using namespace scene_rdl2::math;

//----------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(ColorCorrectHsvMap, scene_rdl2::rdl2::Map)

public:
    ColorCorrectHsvMap(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name);
    ~ColorCorrectHsvMap() override;
    void update() override;

private:
    static void sample(const scene_rdl2::rdl2::Map *self, moonray::shading::TLState *tls,
                       const moonray::shading::State &state, Color *sample);

RDL2_DSO_CLASS_END(ColorCorrectHsvMap)

//----------------------------------------------------------------------------

ColorCorrectHsvMap::ColorCorrectHsvMap(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name):
    Parent(sceneClass, name)
{
    mSampleFunc = ColorCorrectHsvMap::sample;
    mSampleFuncv = (scene_rdl2::rdl2::SampleFuncv) ispc::ColorCorrectHsvMap_getSampleFunc();
}

ColorCorrectHsvMap::~ColorCorrectHsvMap()
{
}

void
ColorCorrectHsvMap::update()
{
}

namespace {

void
applyClamp(Color& result)
{
    result.r = saturate(result.r);
    result.g = saturate(result.g);
    result.b = saturate(result.b);
}

void
computeContrast(float contrastAdjustment, float& channel)
{
    if (contrastAdjustment < 0) {
        channel = 0.5f * contrastAdjustment + channel * (1.f + contrastAdjustment);
    } else if (contrastAdjustment > 0) {
        if (isOne(contrastAdjustment)) contrastAdjustment = 0.999f;
        contrastAdjustment = 1.f / (1.f - contrastAdjustment);
        channel = contrastAdjustment * (channel - 0.5f) + 0.5f;
    }
}

} // anonymous namespace

void
ColorCorrectHsvMap::sample(const scene_rdl2::rdl2::Map *self, moonray::shading::TLState *tls,
                        const moonray::shading::State &state, Color *sample)
{
    const ColorCorrectHsvMap* me = static_cast<const ColorCorrectHsvMap*>(self);

    const Color input = evalColor(me, attrInput, tls, state);
    *sample = input;

    if (!me->get(attrOn)) {
        return;
    }

    Color hsv = rgbToHsv(input);
    float h = hsv.r; float s = hsv.g; float v = hsv.b;

    const float hueShift = evalFloat(me, attrHueShift, tls, state);
    if (!isZero(hueShift)) {
        h = fmod(h + hueShift / 360.f, 1.f);
    }

    const float saturationFactor = evalFloat(me, attrSaturationFactor, tls, state);
    if (!isOne(saturationFactor)) s *= saturationFactor;

    const float valueFactor = evalFloat(me, attrValueFactor, tls, state);
    if (!isOne(valueFactor)) v *= valueFactor;

    const float saturationContrast = clamp(me->get(attrSaturationContrast), -1.f, 1.f);
    computeContrast(saturationContrast, s);

    const float valueContrast = clamp(me->get(attrValueContrast), -1.f, 1.f);
    computeContrast(valueContrast, v);

    const float saturationShift = me->get(attrSaturationShift);
    if (!isZero(saturationShift)) s += saturationShift;

    const float valueShift = me->get(attrValueShift);
    if (!isZero(valueShift)) v += valueShift;

    hsv.r = h; hsv.g = s; hsv.b = v;
    *sample = hsvToRgb(hsv);

    if (me->get(attrClamp)) {
        applyClamp(*sample);
    }
}

