// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ColorCorrectLegacyMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>
#include <scene_rdl2/common/math/ispc/ColorSpace.isph>

enum MonochromeMode {
    MONO_OFF = 0,
    MONO_LUMINANCE = 1,
    MONO_AVERAGE = 2,
    MONO_MINIMUM = 3,
    MONO_MAXIMUM = 4,
    MONO_RED_CHANNEL = 5,
    MONO_GREEN_CHANNEL = 6,
    MONO_BLUE_CHANNEL = 7
};

ISPC_UTIL_EXPORT_ENUM_TO_HEADER(MonochromeMode);


static const varying float
computeLuminanceLegacy(const Color& rgb)
{
    return (0.3f * rgb.r + 0.59f * rgb.g + 0.11f * rgb.b);
}

static void
applyBrightness(const uniform float brightness, varying Color& result)
{
    const uniform Color brightnessColor = Color_ctor(brightness / 255.0f);
    result = result + brightnessColor;
}

static void
applyContrast(uniform float contrast, varying Color& result)
{
    contrast = contrast/100.0f;
    const uniform Color midGray = Color_ctor(0.5f);
    if (contrast < 0) {
        contrast *= -1.0f;
        result = lerp(result, midGray, contrast);
    } else {
        if (isEqual(contrast, 1.0f)) contrast = 0.999f;
        contrast = 1.0f / (1.0f - contrast);
        
        result = ((result - midGray) * contrast) + midGray;
    }
}

static void
applyHueShift(const uniform float hue, varying Color& result)
{
    Color hsv = rgbToHsv(result);
    hsv.r = fmod(hsv.r + (hue / 360.0f), 1.0f);
    result = hsvToRgb(hsv);
}

static void
applySaturation(uniform float saturation, varying Color& result)
{
    saturation = (saturation + 100.0f) / 100.0f;
    const varying Color luminance = Color_ctor(computeLuminanceLegacy(result));
    result = lerp(luminance, result, saturation);
}

static void
applyInversion(varying Color& result)
{
    result.r = 1.0f - result.r;
    result.g = 1.0f - result.g;
    result.b = 1.0f - result.b;
}

static void
applyMonochrome(const uniform int monochrome, varying Color& result)
{
    switch (monochrome) {
    case MONO_LUMINANCE: // Luminance
        const float luminance = computeLuminanceLegacy(result);
        result.r = luminance;
        result.g = result.r;
        result.b = result.r;
        break;
    case MONO_AVERAGE: // Average
        result.r = (result.r + result.g + result.b) / 3.0f;
        result.g = result.r;
        result.b = result.r;
        break;
    case MONO_MINIMUM: // Minimum
        result.r = min(result.r, min(result.g, result.b));
        result.g = result.r;
        result.b = result.r;
        break;
    case MONO_MAXIMUM: // Maximum
        result.r = max(result.r, max(result.g, result.b));
        result.g = result.r;
        result.b = result.r;
        break;
    case MONO_RED_CHANNEL: // Red Channel
        result.g = result.r;
        result.b = result.r;
        break;
    case MONO_GREEN_CHANNEL: // Green Channel
        result.r = result.g;
        result.b = result.g;
        break;
    case MONO_BLUE_CHANNEL: // Blue Channel
        result.r = result.b;
        result.g = result.b;
        break;
    }
}

static void
applyClamp(varying Color& result)
{
    result.r = saturate(result.r);
    result.g = saturate(result.g);
    result.b = saturate(result.b);
}

static Color
sample(const uniform Map* uniform map,
       uniform ShadingTLState* uniform tls,
       const varying State& state)
{
    const Color input = evalAttrInput(map, tls, state);
    Color result = input;

    if (getAttrOn(map) && getAttrMask(map) > 0.0f) {
        const uniform float brightness = getAttrBrightness(map);
        if (!isZero(brightness)) {
            applyBrightness(brightness, result);
        }
            
        const uniform float contrast = getAttrContrast(map);
        if (!isZero(contrast)) {
            applyContrast(contrast, result);
        }

        const uniform float hue = getAttrHue(map);
        if (!isZero(hue)) {
            applyHueShift(hue, result);
        }

        const uniform float saturation = getAttrSaturation(map);
        if (!isZero(saturation)) {
            applySaturation(saturation, result);
        }

        const uniform bool invert = getAttrInvert(map);
        if (invert) {
            applyInversion(result);
        }

        const uniform int monochrome = getAttrMonochrome(map);
        if (!isZero(monochrome)) {
            applyMonochrome(monochrome, result);
        }

        const uniform Color multiplier = getAttrMultiplier(map);
        result = result * multiplier;

        const uniform bool clamp = getAttrClamp(map);
        if (clamp) {
            applyClamp(result);
        }

        const varying float mask = evalAttrMask(map, tls, state);
        result = lerp(result, input, 1.0f - mask);
    }

    return result;
}

DEFINE_MAP_SHADER(ColorCorrectLegacyMap, sample)

