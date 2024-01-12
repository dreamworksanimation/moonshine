// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ColorCorrectHsvMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>
#include <scene_rdl2/common/math/ispc/ColorSpace.isph>

static void
applyClamp(varying Color& result)
{
    result.r = saturate(result.r);
    result.g = saturate(result.g);
    result.b = saturate(result.b);
}

static void
computeContrast(uniform float contrastAdjustment, varying float& channel)
{
    if (contrastAdjustment < 0) {
        channel = 0.5f * contrastAdjustment + channel * (1.f + contrastAdjustment);
    } else if (contrastAdjustment > 0) {
        if (isOne(contrastAdjustment)) contrastAdjustment = 0.999f;
        contrastAdjustment = 1.f / (1.f - contrastAdjustment);
        channel = contrastAdjustment * (channel - 0.5f) + 0.5f;
    }
}

static Color
sample(const uniform Map* uniform map,
       uniform ShadingTLState* uniform tls,
       const varying State& state)
{
    const Color input = evalAttrInput(map, tls, state);
    Color result = input;

    if (!getAttrOn(map)) {
        return result;
    }

    Color hsv = rgbToHsv(input);
    float h = hsv.r; float s = hsv.g; float v = hsv.b;

    const varying float hueShift = evalAttrHueShift(map, tls, state);
    if (!isZero(hueShift)) {
        h = fmod(h + hueShift / 360.f, 1.f);
    }

    const varying float saturationFactor = evalAttrSaturationFactor(map, tls, state);
    if (!isOne(saturationFactor)) s = s * saturationFactor;

    const varying float valueFactor = evalAttrValueFactor(map, tls, state);
    if (!isOne(valueFactor)) v = v * valueFactor;

    const uniform float saturationContrast = clamp(getAttrSaturationContrast(map), -1.f, 1.f);
    computeContrast(saturationContrast, s);

    const uniform float valueContrast = clamp(getAttrValueContrast(map), -1.f, 1.f);
    computeContrast(valueContrast, v);

    const uniform float saturationShift = getAttrSaturationShift(map);
    if (!isZero(saturationShift)) s = s + saturationShift;

    const uniform float valueShift = getAttrValueShift(map);
    if (!isZero(valueShift)) v = v + valueShift;

    hsv.r = h; hsv.g = s; hsv.b = v;
    result = hsvToRgb(hsv);

    if (getAttrClamp(map)) {
        applyClamp(result);
    }

    return result;
}

DEFINE_MAP_SHADER(ColorCorrectHsvMap, sample)

