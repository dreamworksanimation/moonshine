// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file RemapMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>


enum RemapMethod {
    REMAP_UNIFORM = 0,
    REMAP_RGB = 1
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(RemapMethod);

static varying Color
remapRGB(const varying Color& input,
         const uniform Color& inMin, const uniform Color& inMax,
         const uniform Color& outMin, const uniform Color& outMax,
         const uniform Color& biasAmount)
{
    const uniform Color inRange = inMax - inMin;
    const uniform Color outRange = outMax - outMin;

    // Handle certain edge cases.  It is slightly unclear what the behavior
    // should be in these cases, but we need to handle them somehow.
    if (isBlack(outRange)) {
        // both outMin and outMax are the same, so all input values remap to the
        // same output value (outMin)
        return outMin;
    } else if (isBlack(inRange)) {
        // both inMin and inMax are the same, so all input values remap to the same
        // output value.  Start off with average of outMin and outMax, bias if necessary.
        uniform float tr = 0.5f;
        uniform float tg = 0.5f;
        uniform float tb = 0.5f;
        tr = !isEqual(biasAmount.r, 0.5f) ? bias(tr, biasAmount.r) : tr;
        tg = !isEqual(biasAmount.g, 0.5f) ? bias(tg, biasAmount.g) : tg;
        tb = !isEqual(biasAmount.b, 0.5f) ? bias(tb, biasAmount.b) : tb;
    
        return Color_ctor(tr, tg, tb);
    }

    varying float resultR = (input.r - inMin.r) * rcp(inRange.r);
    varying float resultG = (input.g - inMin.g) * rcp(inRange.g);
    varying float resultB = (input.b - inMin.b) * rcp(inRange.b);
    resultR = !isEqual(biasAmount.r, 0.5f) ? bias(resultR, biasAmount.r) : resultR;
    resultG = !isEqual(biasAmount.g, 0.5f) ? bias(resultG, biasAmount.g) : resultG;
    resultB = !isEqual(biasAmount.b, 0.5f) ? bias(resultB, biasAmount.b) : resultB;
    resultR = resultR * outRange.r + outMin.r;
    resultG = resultG * outRange.r + outMin.r;
    resultB = resultB * outRange.r + outMin.r;

    return Color_ctor(resultR, resultG, resultB);
}

static varying Color
remapUniform(const varying Color& input,
             const uniform float inMin, const uniform float inMax,
             const uniform float outMin, const uniform float outMax,
             const uniform bool applyBias,
             const uniform float biasAmount)
{
    const uniform float inRange = inMax - inMin;
    const uniform float outRange = outMax - outMin;

    // Handle certain edge cases.  It is slightly unclear what the behavior
    // should be in these cases, but we need to handle them somehow.
    if (isZero(outRange)) {
        // both outMin and outMax are the same, so all input values remap to the
        // same output value (outMin)
        const float val = outMin;
        return Color_ctor(val, val, val);
    } else if (isZero(inRange)) {
        // both inMin and inMax are the same, so all input values remap to the same
        // output value.  Start off with average of outMin and outMax, bias if necessary.
        float t = 0.5f;
        if (applyBias) {
            t = bias(t, biasAmount);
        }
        const float val = t * outRange + outMin;
        return Color_ctor(val, val, val);
    }

    varying float result[3] = {input.r, input.g, input.b};

    for (uniform int i = 0; i < 3; ++i) {
        float t = (result[i]-inMin) * rcp(inRange);

        if (applyBias) {
            t = bias(t, biasAmount);
        }

        result[i] = t * outRange + outMin;
    }

    return Color_ctor(result[0], result[1], result[2]);
}

static Color
sample(const uniform Map *            uniform map,
             uniform ShadingTLState * uniform tls,
       const varying State &                  state)
{
    const varying Color input      = evalAttrInput(map, tls, state);
    const uniform float inMin      = getAttrInMin(map);
    const uniform float inMax      = getAttrInMax(map);
    const uniform float outMin     = getAttrOutMin(map);
    const uniform float outMax     = getAttrOutMax(map);
    const uniform float biasAmount = getAttrBias(map);

    // going to cast all of these to make them iterable
    const uniform Color inMinRGB      = getAttrInMinRGB(map);
    const uniform Color inMaxRGB      = getAttrInMaxRGB(map);
    const uniform Color outMinRGB     = getAttrOutMinRGB(map);
    const uniform Color outMaxRGB     = getAttrOutMaxRGB(map);
    const uniform Color biasAmountRGB = getAttrBiasRGB(map);

    const uniform bool applyBias = !isEqual(biasAmount, 0.5f);

    varying Color result = input;

    switch(getAttrRemapMethod(map)) {
    case REMAP_UNIFORM:
        if (!isEqual(inMin, outMin) || !isEqual(inMax, outMax)) {
            // apply remapping and potentially bias
            result = remapUniform(result, inMin, inMax, outMin, outMax, applyBias, biasAmount);
        } else if (applyBias) {
            // apply bias only
            result.r = bias(result.r, biasAmount);
            result.g = bias(result.g, biasAmount);
            result.b = bias(result.b, biasAmount);
        }

        if (getAttrClamp(map)) {
            const uniform float clampMin = getAttrClampMin(map);
            const uniform float clampMax = getAttrClampMax(map);
            result.r = clamp(result.r, clampMin, clampMax);
            result.g = clamp(result.g, clampMin, clampMax);
            result.b = clamp(result.b, clampMin, clampMax);
        }
        
        break;
    case REMAP_RGB:
        if (!isEqual(inMinRGB, outMinRGB) || !isEqual(inMaxRGB, outMaxRGB)) {
            // apply remapping and potentially bias
            result = remapRGB(result, inMinRGB, inMaxRGB, outMinRGB,
                              outMaxRGB, biasAmountRGB);
        } else {
            // apply bias only
            result.r = !isEqual(biasAmountRGB.r, 0.5f) ? bias(result.r, biasAmountRGB.r) : result.r;
            result.g = !isEqual(biasAmountRGB.g, 0.5f) ? bias(result.g, biasAmountRGB.g) : result.g;
            result.b = !isEqual(biasAmountRGB.b, 0.5f) ? bias(result.b, biasAmountRGB.b) : result.b;
        }

        if (getAttrClampRGB(map)) {
            const uniform Color clampMinRGB = getAttrClampMinRGB(map);
            const uniform Color clampMaxRGB = getAttrClampMaxRGB(map);
            result.r = clamp(result.r, clampMinRGB.r, clampMaxRGB.r);
            result.g = clamp(result.g, clampMinRGB.g, clampMaxRGB.g);
            result.b = clamp(result.b, clampMinRGB.b, clampMaxRGB.b);
        }
        
        break;
    default:
        break;
    }

    return result;
}
DEFINE_MAP_SHADER(RemapMap, sample)

