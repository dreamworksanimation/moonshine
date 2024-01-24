// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file OpMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

Color
colorDivide(const varying Color &a, const varying Color &b)
{
    const varying Color denom = max(b, Color_ctor(0.00001f));
    return Color_ctor(
        a.r / denom.r,
        a.g / denom.g,
        a.b / denom.b
    );
}

Color
colorCross(const varying Color &a, const varying Color &b)
{
    const varying Vec3f vecA = Vec3f_ctor(a.r, a.g, a.b);
    const varying Vec3f vecB = Vec3f_ctor(b.r, b.g, b.b);
    const varying Vec3f vecC = cross(vecA, vecB);
    return Color_ctor(vecC.x, vecC.y, vecC.z);
}

Color
colorDot(const varying Color &a, const varying Color &b)
{
    const varying Vec3f vecA = Vec3f_ctor(a.r, a.g, a.b);
    const varying Vec3f vecB = Vec3f_ctor(b.r, b.g, b.b);
    const varying float dotResult = dot(vecA, vecB);
    return Color_ctor(dotResult, dotResult, dotResult);
}

Color
colorNormalize(const varying Color &a)
{
    Vec3f vecA = Vec3f_ctor(a.r, a.g, a.b);
    vecA = normalize(vecA);
    return Color_ctor(vecA.x, vecA.y, vecA.z);
}

Color
colorOverlay(const varying Color &a, const varying Color &b)
{
    const varying float A[3] = {a.r, a.g, a.b};
    const varying float B[3] = {b.r, b.g, b.b};
    float res[3];

    for (size_t i = 0; i < 3; ++i) {
        if (B[i] < 0.5)
            res[i] = A[i] * B[i] * 2.0;
        else
            res[i] = 1.0 -  (1.0 - A[i]) * (1.0 - B[i]) * 2.0;
    }

    return clamp(Color_ctor(res[0], res[1], res[2]), 0.0, 1.0);
}

Color
colorScreen(const varying Color &a, const varying Color &b)
{
    Color result;
    result.r = 1.0 - (1.0 - a.r) * (1.0 - b.r);
    result.g = 1.0 - (1.0 - a.g) * (1.0 - b.g);
    result.b = 1.0 - (1.0 - a.b) * (1.0 - b.b);
    return clamp(result, 0.0, 1.0);
}

Color
colorAbs(const varying Color &a)
{
    Color result;
    result.r = abs(a.r);
    result.g = abs(a.g);
    result.b = abs(a.b);
    return result;
}

Color
colorCeil(const varying Color &a)
{
    Color result;
    result.r = ceil(a.r);
    result.g = ceil(a.g);
    result.b = ceil(a.b);
    return result;
}

Color
colorFloor(const varying Color &a)
{
    Color result;
    result.r = floor(a.r);
    result.g = floor(a.g);
    result.b = floor(a.b);
    return result;
}

Color
colorModulo(const varying Color &a, const varying Color &b)
{
    Color result;
    // We need to clamp the divisor to avoid a crash and
    // clamping to sEpsilon still crashes
    result.r = fmod(a.r, max(0.00001f, b.r));
    result.g = fmod(a.g, max(0.00001f, b.g));
    result.b = fmod(a.b, max(0.00001f, b.b));
    return result;
}

Color
colorFrac(const varying Color &a)
{
    Color result;
    result.r = a.r - floor(a.r);
    result.g = a.g - floor(a.g);
    result.b = a.b - floor(a.b);
    return result;
}

Color
colorLength(const varying Color &a)
{
    Vec3f vecA = Vec3f_ctor(a.r, a.g, a.b);
    float l = length(vecA);
    return Color_ctor(l, l, l);
}

Color
colorSin(const varying Color &a)
{
    Color result;
    result.r = sin(a.r);
    result.g = sin(a.g);
    result.b = sin(a.b);
    return result;
}

Color
colorCos(const varying Color &a)
{
    Color result;
    result.r = cos(a.r);
    result.g = cos(a.g);
    result.b = cos(a.b);
    return result;
}

Color
colorAcos(const varying Color &a)
{
    Color result;
    // We need to clamp to -1 < x < 1 to avoid a crash
    result.r = dw_acos(clamp(a.r, (-1.0f + sEpsilon), (1.0f - sEpsilon)));
    result.g = dw_acos(clamp(a.g, (-1.0f + sEpsilon), (1.0f - sEpsilon)));
    result.b = dw_acos(clamp(a.b, (-1.0f + sEpsilon), (1.0f - sEpsilon)));
    return result;
}

float
floatAlmostEquals(const varying float a, const varying float b, const uniform float epsilon)
{
    return abs(a - b) <= epsilon ? 1.0f : 0.0f;
}

enum OpMapType {
    ADD                   = 0,
    SUBTRACT              = 1,
    MULTIPLY              = 2,
    DIVIDE                = 3,
    MAXIMUM               = 4,
    MINIMUM               = 5,
    POWER                 = 6,
    CROSS                 = 7,
    DOT                   = 8,
    INVERT                = 9,
    NORMALIZE             = 10,
    OP1                   = 11,
    OP2                   = 12,
    OVERLAY               = 13,
    SCRN                  = 14,
    ABS                   = 15,
    CEIL                  = 16,
    FLOOR                 = 17,
    MODULO                = 18,
    FRACTION              = 19,
    LENGTH                = 20,
    SINE                  = 21,
    COSINE                = 22,
    ROUND                 = 23,
    ACOS                  = 24,
    LESS_THAN             = 25,
    LESS_THAN_OR_EQUAL    = 26,
    GREATER_THAN          = 27,
    GREATER_THAN_OR_EQUAL = 28,
    EQUAL                 = 29,
    NOT_EQUAL             = 30,
    AND                   = 31,
    OR                    = 32,
    NOT                   = 33,
    XOR                   = 34,
    BIT_SHIFT_LEFT        = 35,
    BIT_SHIFT_RIGHT       = 36,
    BITWISE_AND           = 37,
    BITWISE_OR            = 38,
    VECTOR_EQUAL          = 39,
    VECTOR_NOT_EQUAL      = 40
};

ISPC_UTIL_EXPORT_ENUM_TO_HEADER(OpMapType);

static Color
sample(const uniform Map *uniform map,
       uniform ShadingTLState * uniform tls,
       const varying State &state)
{
    const uniform int operation = getAttrOperation(map);
    const Color op1 = evalAttrOp1(map, tls, state);
    const Color op2 = evalAttrOp2(map, tls, state);
    const float op1Factor = evalAttrOp1Factor(map, tls, state);
    const float op2Factor = evalAttrOp2Factor(map, tls, state);
    const uniform bool doClamp = getAttrClamp(map);

    const Color opOne = op1 * op1Factor;
    const Color opTwo = op2 * op2Factor;

    const uniform float tolerance = getAttrTolerance(map);

    Color result = op1;

    switch(operation) {
        case ADD:
            result = opOne + opTwo;
            break;
        case SUBTRACT:
            result = opOne - opTwo;
            break;
        case MULTIPLY:
            result = opOne * opTwo;
            break;
        case DIVIDE:
            result = colorDivide(opOne, opTwo);
            break;
        case INVERT:
            result.r = 1.0f - opOne.r;
            result.g = 1.0f - opOne.g;
            result.b = 1.0f - opOne.b;
            break;
        case MAXIMUM:
            result = max(opOne, opTwo);
            break;
        case MINIMUM:
            result = min(opOne, opTwo);
            break;
        case POWER:
            Color base = max(opOne, 
                Color_ctor(sEpsilon));
            result.r = pow(base.r, opTwo.r);
            result.g = pow(base.g, opTwo.g);
            result.b = pow(base.b, opTwo.b);
            break;
        case CROSS:
            result = colorCross(opOne, opTwo);
            break;
        case DOT:
            result = colorDot(opOne, opTwo);
            break;
        case NORMALIZE:
            result = colorNormalize(opOne);
            break;
        case OP1:
            result = opOne;
            break;
        case OP2:
            result = opTwo;
            break;
        case OVERLAY:
            result = colorOverlay(opOne, opTwo);
            break;
        case SCRN:
            result = colorScreen(opOne, opTwo);
            break;
        case ABS:
            result = colorAbs(opOne);
            break;
        case SINE:
            result = colorSin(opOne);
            break;
        case COSINE:
            result = colorCos(opOne);
            break;
        case ACOS:
            result = colorAcos(opOne);
            break;
        case CEIL:
            result = colorCeil(opOne);
            break;
        case FLOOR:
            result = colorFloor(opOne);
            break;
        case MODULO:
            result = colorModulo(opOne, opTwo);
            break;
        case FRACTION:
            result = colorFrac(opOne);
            break;
        case LENGTH:
            result = colorLength(opOne);
            break;
        case ROUND:
            result.r = (float)(int)(opOne.r < 0.0f ? opOne.r - 0.5f : opOne.r + 0.5f);
            result.g = (float)(int)(opOne.g < 0.0f ? opOne.g - 0.5f : opOne.g + 0.5f);
            result.b = (float)(int)(opOne.b < 0.0f ? opOne.b - 0.5f : opOne.b + 0.5f);
            break;
        case LESS_THAN:
            result.r = (float)(opOne.r < opTwo.r);
            result.g = (float)(opOne.g < opTwo.g);
            result.b = (float)(opOne.b < opTwo.b);
            break;
        case LESS_THAN_OR_EQUAL:
            result.r = (float)(opOne.r <= opTwo.r);
            result.g = (float)(opOne.g <= opTwo.g);
            result.b = (float)(opOne.b <= opTwo.b);
            break;
        case GREATER_THAN:
            result.r = (float)(opOne.r > opTwo.r);
            result.g = (float)(opOne.g > opTwo.g);
            result.b = (float)(opOne.b > opTwo.b);
            break;
        case GREATER_THAN_OR_EQUAL:
            result.r = (float)(opOne.r >= opTwo.r);
            result.g = (float)(opOne.g >= opTwo.g);
            result.b = (float)(opOne.b >= opTwo.b);
            break;
        case EQUAL:
            result.r = floatAlmostEquals(opOne.r, opTwo.r, tolerance);
            result.g = floatAlmostEquals(opOne.g, opTwo.g, tolerance);
            result.b = floatAlmostEquals(opOne.b, opTwo.b, tolerance);
            break;
        case NOT_EQUAL:
            result.r = 1.0f - floatAlmostEquals(opOne.r, opTwo.r, tolerance);
            result.g = 1.0f - floatAlmostEquals(opOne.g, opTwo.g, tolerance);
            result.b = 1.0f - floatAlmostEquals(opOne.b, opTwo.b, tolerance);
            break;
        case AND:
            result.r = (float)(opOne.r && opTwo.r);
            result.g = (float)(opOne.g && opTwo.g);
            result.b = (float)(opOne.b && opTwo.b);
            break;
        case OR:
            result.r = (float)(opOne.r || opTwo.r);
            result.g = (float)(opOne.g || opTwo.g);
            result.b = (float)(opOne.b || opTwo.b);
            break;
        case NOT:
            result.r = (float)(!opOne.r);
            result.g = (float)(!opOne.g);
            result.b = (float)(!opOne.b);
            break;
        case XOR:
            result.r = (float)((int)opOne.r ^ (int)opTwo.r);
            result.g = (float)((int)opOne.g ^ (int)opTwo.g);
            result.b = (float)((int)opOne.b ^ (int)opTwo.b);
            break;
        case BIT_SHIFT_LEFT:
            result.r = (float)((int)opOne.r << (int)opTwo.r);
            result.g = (float)((int)opOne.g << (int)opTwo.g);
            result.b = (float)((int)opOne.b << (int)opTwo.b);
            break;
        case BIT_SHIFT_RIGHT:
            // This gives ispc warning:
            // Performance Warning: Shift right is inefficient for varying shift amounts.
            result.r = (float)((int)opOne.r >> (int)opTwo.r);
            result.g = (float)((int)opOne.g >> (int)opTwo.g);
            result.b = (float)((int)opOne.b >> (int)opTwo.b);
            break;
        case BITWISE_AND:
            result.r = (float)((int)opOne.r & (int)opTwo.r);
            result.g = (float)((int)opOne.g & (int)opTwo.g);
            result.b = (float)((int)opOne.b & (int)opTwo.b);
            break;
        case BITWISE_OR:
            result.r = (float)((int)opOne.r | (int)opTwo.r);
            result.g = (float)((int)opOne.g | (int)opTwo.g);
            result.b = (float)((int)opOne.b | (int)opTwo.b);
            break;
        case VECTOR_EQUAL:
            Color distanceC = colorLength(opOne - opTwo);
            float distance = distanceC.r;
            result = Color_ctor(floatAlmostEquals(distance, 0.0f, tolerance));
            break;
        case VECTOR_NOT_EQUAL:
            Color distanceC1 = colorLength(opOne - opTwo);
            float distance1 = distanceC1.r;
            result = Color_ctor(1.0f - floatAlmostEquals(distance1, 0.0f, tolerance));
            break;
        default:
            result = Color_ctor(0.0);
            break;
    }

    if (doClamp) {
        result = clamp(result, 0.f, 1.f);
    }

    return result;
}

DEFINE_MAP_SHADER(OpMap, sample)

