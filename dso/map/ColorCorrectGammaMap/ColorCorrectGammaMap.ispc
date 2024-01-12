// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ColorCorrectGammaMap.ispc

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

    if (!isEqual(mix, 1.0f)) {
        sample.r = lerpOpt(input.r, sample.r, mix);
        sample.g = lerpOpt(input.g, sample.g, mix);
        sample.b = lerpOpt(input.b, sample.b, mix);
    }

    return sample;
}

DEFINE_MAP_SHADER(ColorCorrectGammaMap, sample)

