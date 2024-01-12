// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ColorCorrectMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>
#include <moonray/rendering/shading/ispc/ColorCorrect.isph>

static Color
sample(const uniform Map* uniform map,
       uniform ShadingTLState* uniform tls,
       const varying State& state)
{
    const varying Color input = evalAttrInput(map, tls, state);
    varying Color sample = input;

    const varying float mix = evalAttrMix(map, tls, state);
    if (!getAttrOn(map) || isZero(mix)) {
        return sample;
    }

    if (getAttrHueShiftEnabled(map)) {
        const varying float hueShift = evalAttrHueShift(map, tls, state);
        applyHueShift(hueShift, sample);
    }
    
    if (getAttrSaturationEnabled(map)) {
        varying Color saturation;
        if (getAttrSaturationPerChannel(map)) {
            saturation = Color_ctor(evalAttrSaturationR(map, tls, state),
                                    evalAttrSaturationG(map, tls, state),
                                    evalAttrSaturationB(map, tls, state));
        } else {
            const float saturationFloat = evalAttrSaturation(map, tls, state);
            saturation = Color_ctor(saturationFloat);
        }
        applySaturation(saturation.r, saturation.g, saturation.b,
                        sample.r, sample.g, sample.b);
    }

    if (getAttrContrastEnabled(map)) {
        varying Color contrast;
        if (getAttrContrastPerChannel(map)) {
            contrast = Color_ctor(evalAttrContrastR(map, tls, state),
                                  evalAttrContrastG(map, tls, state),
                                  evalAttrContrastB(map, tls, state));
        } else {
            const float contrastFloat = evalAttrContrast(map, tls, state);
            contrast = Color_ctor(contrastFloat);
        }
        applyContrast(contrast, sample);
    }

    if (getAttrGammaEnabled(map)) {
        varying Color gamma;
        if (getAttrGammaPerChannel(map)) {
            gamma = Color_ctor(evalAttrGammaR(map, tls, state),
                               evalAttrGammaG(map, tls, state),
                               evalAttrGammaB(map, tls, state));
        } else {
            const float gammaFloat = evalAttrGamma(map, tls, state);
            gamma = Color_ctor(gammaFloat);
        }

        varying Color epsilonColor = Color_ctor(sEpsilon);
        // invGam = 1.0f / max(epsilon, gam)
        varying Color invGamma = max(epsilonColor, gamma);
        invGamma = rcp(invGamma);
        applyGamma(invGamma.r, invGamma.g, invGamma.b,
                   sample.r, sample.g, sample.b);
    }

    if (getAttrGainOffsetEnabled(map)) {
        varying Color gain;
        varying Color offset;
        if (getAttrGainOffsetPerChannel(map)) {
            gain = Color_ctor(evalAttrGainR(map, tls, state),
                              evalAttrGainG(map, tls, state),
                              evalAttrGainB(map, tls, state));
            offset = Color_ctor(evalAttrOffsetR(map, tls, state),
                                evalAttrOffsetG(map, tls, state),
                                evalAttrOffsetB(map, tls, state));
        } else {
            const float gainFloat = evalAttrGain(map, tls, state);
            const float offsetFloat = evalAttrOffset(map, tls, state);
            gain = Color_ctor(gainFloat);
            offset = Color_ctor(offsetFloat);
        }

        applyGainAndOffset( gain.r, gain.g, gain.b,
                            offset.r, offset.g, offset.b,
                            sample.r, sample.g, sample.b);
    }
    
    if (getAttrTMIControlEnabled(map)) {
        varying Color tmiColor = evalAttrTMI(map, tls, state);
        applyTMI(tmiColor.r, tmiColor.g, tmiColor.b, sample.r, sample.g, sample.b);
    }
    
    if (!isEqual(mix, 1.0f)) {
        sample.r = lerpOpt(input.r, sample.r, mix);
        sample.g = lerpOpt(input.g, sample.g, mix);
        sample.b = lerpOpt(input.b, sample.b, mix);
    }

    if (getAttrClamp(map)) {
        sample.r = clamp(sample.r, getAttrClampMin(map), getAttrClampMax(map));
        sample.g = clamp(sample.g, getAttrClampMin(map), getAttrClampMax(map));
        sample.b = clamp(sample.b, getAttrClampMin(map), getAttrClampMax(map));
    }

    return sample;
}

DEFINE_MAP_SHADER(ColorCorrectMap, sample)

