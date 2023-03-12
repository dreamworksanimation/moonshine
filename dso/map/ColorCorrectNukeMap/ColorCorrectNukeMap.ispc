// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ColorCorrectNukeMap.ispc

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
    
    if (getAttrSaturationEnabled(map)) {
        varying Color sat = getAttrSaturation(map);
        applySaturation(sat.r, sat.g, sat.b,
                        sample.r, sample.g, sample.b);
    }
    
    if (getAttrContrastEnabled(map)) {
        varying Color contrast = getAttrContrast(map);
        applyNukeContrast(  contrast.r, contrast.g, contrast.b,
                            sample.r, sample.g, sample.b);
    }
    
    if (getAttrGammaEnabled(map)) {
        varying Color epsilon = Color_ctor(sEpsilon);
        varying Color gam = getAttrGamma(map);

        // invGam = 1.0f / max(epsilon, gam)
        varying Color invGam = max(epsilon, gam);
        invGam = rcp(invGam);
        applyGamma( invGam.r, invGam.g, invGam.b,
                    sample.r, sample.g, sample.b);
    }
    
    if (getAttrGainOffsetEnabled(map)) {
        varying Color gain = getAttrGain(map);
        varying Color offset = getAttrOffset(map);
        applyGainAndOffset( gain.r, gain.g, gain.b,
                            offset.r, offset.g, offset.b,
                            sample.r, sample.g, sample.b);
    }
    
    if (getAttrTMIControlEnabled(map)) {
        // The TMI parameter is kept as a Vec3f for backward 
        // compatibility so we need to convert it to a color
        // to pass to the applyTMI function
        varying Vec3f tmiVec = getAttrTMI(map);
        varying Color tmiColor = Color_ctor(tmiVec.x, tmiVec.y, tmiVec.z);
        applyTMI(tmiColor.r, tmiColor.g, tmiColor.b, sample.r, sample.g, sample.b);
    }
    
    return sample;
}

DEFINE_MAP_SHADER(ColorCorrectNukeMap, sample)

