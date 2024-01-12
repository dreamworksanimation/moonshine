// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ColorCorrectLegacyMap.cc

#include "attributes.cc"
#include "ColorCorrectLegacyMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>
#include <scene_rdl2/common/math/ColorSpace.h>

using namespace moonray::shading;
using namespace scene_rdl2::math;


//----------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(ColorCorrectLegacyMap, scene_rdl2::rdl2::Map)

public:
    ColorCorrectLegacyMap(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name);
    ~ColorCorrectLegacyMap() override;
    void update() override;

private:
    static void sample(const scene_rdl2::rdl2::Map *self, moonray::shading::TLState *tls,
                       const moonray::shading::State &state, Color *sample);

RDL2_DSO_CLASS_END(ColorCorrectLegacyMap)

//----------------------------------------------------------------------------

ColorCorrectLegacyMap::ColorCorrectLegacyMap(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name):
    Parent(sceneClass, name)
{
    mSampleFunc = ColorCorrectLegacyMap::sample;
    mSampleFuncv = (scene_rdl2::rdl2::SampleFuncv) ispc::ColorCorrectLegacyMap_getSampleFunc();
}

ColorCorrectLegacyMap::~ColorCorrectLegacyMap()
{
}

void
ColorCorrectLegacyMap::update()
{
}

namespace {
inline float
computeLuminanceLegacy(const Color& rgb)
{
    return rgb.r * 0.3f + rgb.g * 0.59f + rgb.b * 0.11f;
}

void
applyBrightness(float brightness, Color& result)
{
    brightness /= 255.0f;
    result.r += brightness;
    result.g += brightness;
    result.b += brightness;
}

void
applyContrast(float contrast, Color& result)
{
    contrast /= 100.0f;
    if (contrast < 0) {
        contrast *= -1.0f;
        result.r = lerp(result.r, 0.5f, contrast);
        result.g = lerp(result.g, 0.5f, contrast);
        result.b = lerp(result.b, 0.5f, contrast);
    } else {
        if (isEqual(contrast, 1.0f)) contrast = 0.999f;
        contrast = 1.0f / (1.0f - contrast);
        result.r = contrast * (result.r - 0.5f) + 0.5f;
        result.g = contrast * (result.g - 0.5f) + 0.5f;
        result.b = contrast * (result.b - 0.5f) + 0.5f;
    }
}

void
applyHueShift(const float hue, Color& result)
{
    Color hsv = rgbToHsv(result);
    hsv.r = scene_rdl2::math::fmod(hsv.r + hue / 360.0f, 1.0f);
    result = hsvToRgb(hsv);
}

void
applySaturation(float saturation, Color& result)
{
    saturation = (saturation + 100.0f) / 100.0f;
    Color luminance = Color(computeLuminanceLegacy(result));
    result = lerp(luminance, result, saturation);
}

void
applyInversion(Color& result)
{
    result.r = 1.0f - result.r;
    result.g = 1.0f - result.g;
    result.b = 1.0f - result.b;
}

void
applyMonochrome(const int monochrome, Color& result)
{
    switch (monochrome) {
    case ispc::MONO_LUMINANCE: // Luminance
        result.r = computeLuminanceLegacy(result);
        result.g = result.r;
        result.b = result.r;
        break;
    case ispc::MONO_AVERAGE: // Average
        result.r = (result.r + result.g + result.b) / 3.0f;
        result.g = result.r;
        result.b = result.r;
        break;
    case ispc::MONO_MINIMUM: // Minimum
        result.r = min(result.r, min(result.g, result.b));
        result.g = result.r;
        result.b = result.r;
        break;
    case ispc::MONO_MAXIMUM: // Maximum
        result.r = max(result.r, max(result.g, result.b));
        result.g = result.r;
        result.b = result.r;
        break;
    case ispc::MONO_RED_CHANNEL: // Red Channel
        result.g = result.r;
        result.b = result.r;
        break;
    case ispc::MONO_GREEN_CHANNEL: // Green Channel
        result.r = result.g;
        result.b = result.g;
        break;
    case ispc::MONO_BLUE_CHANNEL: // Blue Channel
        result.r = result.b;
        result.g = result.b;
        break;
    }
}

void
applyClamp(Color& result)
{
    result.r = saturate(result.r);
    result.g = saturate(result.g);
    result.b = saturate(result.b);
}

} // anonymous namespace

void
ColorCorrectLegacyMap::sample(const scene_rdl2::rdl2::Map *self, moonray::shading::TLState *tls,
                        const moonray::shading::State &state, Color *sample)
{
    const ColorCorrectLegacyMap* me = static_cast<const ColorCorrectLegacyMap*>(self);

    const Color input = evalColor(me, attrInput, tls, state);
    Color result(input);

    if (me->get(attrOn) && evalFloat(me, attrMask, tls, state) > 0.0f) {
        const float brightness = me->get(attrBrightness);
        if (!isZero(brightness)) {
            applyBrightness(brightness, result);
        }

        const float contrast = me->get(attrContrast);
        if (!isZero(contrast)) {
            applyContrast(contrast, result);
        }

        const float hue = me->get(attrHue);
        if (!isZero(hue)) {
            applyHueShift(hue, result);
        }

        const float saturation = me->get(attrSaturation);
        if (!isZero(saturation)) {
            applySaturation(saturation, result);
        }

        const bool invert = me->get(attrInvert);
        if (invert) {
            applyInversion(result);
        }

        const int monochrome = me->get(attrMonochrome);
        if (monochrome != 0) {
            applyMonochrome(monochrome, result);
        }

        const Color multiplier = me->get(attrMultiplier);
        result *= multiplier;

        const bool clamp = me->get(attrClamp);
        if (clamp) {
            applyClamp(result);
        }

        const float mask = evalFloat(me, attrMask, tls, state);
        result = lerp(result, input, 1.0f - mask);
    }

    *sample = result;
}

