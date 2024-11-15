// Copyright 2023-2024 DreamWorks Animation LLC
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
         const varying Color& inMin, const varying Color& inMax,
         const varying Color& outMin, const varying Color& outMax,
         const varying Color& biasAmount)
{
    const varying Color inRange = inMax - inMin;
    const varying Color outRange = outMax - outMin;

    // Handle certain edge cases.  It is slightly unclear what the behavior
    // should be in these cases, but we need to handle them somehow.
    if (isBlack(outRange)) {
        // both outMin and outMax are the same, so all input values remap to the
        // same output value (outMin)
        return outMin;
    } else if (isBlack(inRange)) {
        // both inMin and inMax are the same, so all input values remap to the same
        // output value.  Start off with average of outMin and outMax, bias if necessary.
        varying float tr = 0.5f;
        varying float tg = 0.5f;
        varying float tb = 0.5f;
        tr = !isEqual(biasAmount.r, 0.5f) ? bias(tr, biasAmount.r) : tr;
        tg = !isEqual(biasAmount.g, 0.5f) ? bias(tg, biasAmount.g) : tg;
        tb = !isEqual(biasAmount.b, 0.5f) ? bias(tb, biasAmount.b) : tb;
    
        return Color_ctor(tr, tg, tb);
    }

    varying Color result = (input - inMin) / inRange;
    result.r = bias(result.r, biasAmount.r);
    result.g = bias(result.g, biasAmount.g);
    result.b = bias(result.b, biasAmount.b);
    return result * outRange + outMin;
}

static varying Color
remapUniform(const varying Color& input,
             const varying float inMin, const varying float inMax,
             const varying float outMin, const varying float outMax,
             const varying bool applyBias,
             const varying float biasAmount)
{
    const varying float inRange = inMax - inMin;
    const varying float outRange = outMax - outMin;

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

    Color result = (input - inMin) / inRange;
    if (applyBias) {
        result.r = bias(result.r, biasAmount);
        result.g = bias(result.g, biasAmount);
        result.b = bias(result.b, biasAmount);
    }
    return result * outRange + outMin;
}

static Color
sample(const uniform Map *            uniform map,
             uniform ShadingTLState * uniform tls,
       const varying State &                  state)
{
    const varying Color input      = evalAttrInput(map, tls, state);
    const varying float inMin      = evalAttrInMin(map, tls, state);
    const varying float inMax      = evalAttrInMax(map, tls, state);
    const varying float outMin     = evalAttrOutMin(map, tls, state);
    const varying float outMax     = evalAttrOutMax(map, tls, state);
    const varying float biasAmount = evalAttrBias(map, tls, state);

    // going to cast all of these to make them iterable
    const varying Color inMinRGB      = evalAttrInMinRGB(map, tls, state);
    const varying Color inMaxRGB      = evalAttrInMaxRGB(map, tls, state);
    const varying Color outMinRGB     = evalAttrOutMinRGB(map, tls, state);
    const varying Color outMaxRGB     = evalAttrOutMaxRGB(map, tls, state);
    const varying Color biasAmountRGB = evalAttrBiasRGB(map, tls, state);

    const varying bool applyBias = !isEqual(biasAmount, 0.5f);

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

