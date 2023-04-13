// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file GradientMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

enum GradientFalloffType {
    GRADIENT_FALLOFF_NONE = 0,
    GRADIENT_FALLOFF_NATURAL = 1,
    GRADIENT_FALLOFF_LINEAR = 2,
    GRADIENT_FALLOFF_SQUARED = 3,
    GRADIENT_FALLOFF_GAUSSIAN = 4,
    GRADIENT_FALLOFF_EASE_OUT = 5
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(GradientFalloffType);

static const uniform int FALLOFF_HEAD = 1;
static const uniform int FALLOFF_TAIL = 7;

// f, g, and h, are helper functions for naturalFalloff().
static varying float
f(const varying float &x, const uniform float power)
{
    return 1.0f / pow(x, power);
}

static uniform float
f(const uniform float &x, const uniform float power)
{
    return 1.0f / pow(x, power);
}

static varying float
g(const varying float &x, const uniform float power)
{
    return f(FALLOFF_TAIL * x + FALLOFF_HEAD, power);
}

static uniform float
g(const uniform float &x, const uniform float power)
{
    return f(FALLOFF_TAIL * x + FALLOFF_HEAD, power);
}

static varying float
h(const varying float &x, const uniform float power)
{
    float a = g(x, power);
    uniform float b = g(1.0f, power);
    return a - b;
}

static uniform float
h(const uniform float &x, const uniform float power)
{
    uniform float a = g(x, power);
    uniform float b = g(1.0f, power);
    return a - b;
}

// Helper function for NATURAL falloff type.
static varying float
naturalFalloff(const varying float &x, const uniform float power)
{
    float a = h(x, power);
    float b = h(0.0f, power);
    return a / b;
}


varying float
computeFalloff(const varying float &t, const uniform GradientFalloffType type, const uniform float exponent)
{
    float h = t;
    switch (type) {
    case GRADIENT_FALLOFF_NONE:
        h = 1.0f;
        break;
    case GRADIENT_FALLOFF_NATURAL:
        h = naturalFalloff(h, exponent);
        break;
    case GRADIENT_FALLOFF_LINEAR:
        h = pow(h, exponent);
        break;
    case GRADIENT_FALLOFF_SQUARED:
        h = h * h;
        break;
    case GRADIENT_FALLOFF_GAUSSIAN:
        h = pow(((cos((1.0f - h) * (float)sPi) + 1.0f) / 2.0f), exponent);
        break;
    case GRADIENT_FALLOFF_EASE_OUT:
        h = 3.0f * h * h - 2.0f * h * h * h;
        h = pow(h, exponent);
        break;
    }
    return h;
}

struct StaticGradientMapData {
    // shade time error events - these can be static as long as we
    // ensure they have the same value across all instances.
    // no conditional registration of log events is allowed.
    uniform int sErrorMissingReferenceData;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(StaticGradientMapData);

struct GradientMap
{
    const Xform *mXform;
    uniform int mRefPKey;

    uniform Color mFatalColor;
    uniform StaticGradientMapData* uniform mGradientMapDataPtr;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(GradientMap);

static Color
sample(const uniform Map* uniform map,
       uniform ShadingTLState* uniform tls,
       const varying State& state)
{
    const uniform GradientMap * uniform me = MAP_GET_ISPC_CPTR(GradientMap, map);

    const varying Color colorA = evalAttrColorA(map, tls, state);
    const varying Color colorB = evalAttrColorB(map, tls, state);
    const uniform Vec3f start = getAttrStart(map);
    const uniform Vec3f end = getAttrEnd(map);
    const uniform float falloffStart = getAttrFalloffStart(map);
    const uniform float falloffEnd = getAttrFalloffEnd(map);
    const uniform int falloffType = getAttrFalloffType(map);
    const uniform float falloffExponent = getAttrFalloffExponent(map);
    const uniform float falloffEndIntensity = getAttrFalloffEndIntensity(map);
    const uniform float falloffBias = getAttrFalloffBias(map);
    const uniform bool isSymmetric = getAttrSymmetric(map);
    uniform float symmetricCenter = getAttrSymmetricCenter(map);

    // Get the position
    varying Vec3f pos;
    const uniform int space = getAttrSpace(map);
    if (space == SHADING_SPACE_TEXTURE) {
        pos = Vec3f_ctor(
            state.mSt.x,
            state.mSt.y,
            0.0f
        );
    } else if (space == SHADING_SPACE_REFERENCE) {
        SHADER_DATA_getRefP(me->mRefPKey, tls, state, &pos);
    } else {
        pos = getP(state);

        if (space != SHADING_SPACE_RENDER) {
            pos = transformPoint(
                me->mXform, 
                SHADING_SPACE_RENDER,
                space, 
                state, 
                pos
            );
        }
    }

    // blend is the scalar projection of P onto v.
    varying float blend = 0.0f;
    const uniform Vec3f v = end - start;
    const uniform float magSq = lengthSqr(v);

    if (magSq > 0.0f) {
        const varying Vec3f ps = pos - start;
        blend = saturate(dot(ps, v) / magSq);
    }

    // Remap blend value between falloffStart and falloffEnd values.
    if (blend < falloffStart) {
        blend = 0.0f;
    } else if (blend > falloffEnd) {
        blend = 1.0f;
    } else {
        blend = saturate((blend - falloffStart) / (falloffEnd - falloffStart));
    }

    // Natural falloff curve is reversed from others, so we don't flip it.
    if (falloffType != GRADIENT_FALLOFF_NATURAL) blend = 1.0f - blend;
    blend = (1.0f - computeFalloff(blend, (uniform GradientFalloffType)falloffType, falloffExponent));

    // colorA blends into colorB and then back into colorA.
    // The blend from colorA to colorB and back to colorA
    // occurs in the same amount of distance as blending from
    // colorA to colorB without symmetric mode on.
    if (isSymmetric) {
        // Clamp to exclusive (0,1) range.
        symmetricCenter = clamp(symmetricCenter, sEpsilon, sOneMinusEpsilon);

        if (blend < symmetricCenter) {
            blend = blend / symmetricCenter;
        } else {
            blend = 1.0f - (blend - symmetricCenter) / (1.0f - symmetricCenter);
        }
    }
    blend = blend * saturate(falloffEndIntensity);
    blend = bias(blend, falloffBias);
    const varying Color sample = lerp(colorA, colorB, blend);
    return sample;
}

DEFINE_MAP_SHADER(GradientMap, sample)

