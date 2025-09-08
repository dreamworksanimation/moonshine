// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.isph"

#include <moonray/common/noise/ispc/Simplex.isph>
#include <moonray/map/primvar/ispc/Primvar.isph>

#include <moonray/rendering/shading/ispc/MapApi.isph>

struct DistortNormalMap
{
    const uniform NOISE_Simplex * uniform mNoiseU;
    const uniform NOISE_Simplex * uniform mNoiseV;
    uniform int mRefPKey;
    uniform int mHairSurfaceSTKey;
    uniform int mHairClosestSurfaceSTKey;

    uniform int sErrorMissingReferenceData;
    const uniform NormalMap * uniform mNormalMap;
    uniform intptr_t mSampleNormalFunc;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(DistortNormalMap);

// Rodrigues' rotation formula, assume axis is normalized
Vec3f rotateVector(const Vec3f& v, const Vec3f& axis, float theta) {
    float ct, st;
    sincos(theta, &st, &ct);
    return ct * v + st * cross(axis, v) + dot(axis, v) * (1.f - ct) * axis;
}

static Vec3f
sampleNormal(const uniform NormalMap* uniform map,
             uniform ShadingTLState* uniform tls,
             const varying State& state)
{
    const uniform DistortNormalMap* uniform me = NORMALMAP_GET_ISPC_CPTR(DistortNormalMap, map);

    varying Vec3f n = getN(state);
    SampleNormalPtr sampleNormalFn = (SampleNormalPtr) me->mSampleNormalFunc;
    if (sampleNormalFn != nullptr) {
        sampleNormalFn(me->mNormalMap, tls, &state, &n);
    }

    varying Vec3f u = getdPds(state);
    varying Vec3f v = getdPdt(state);

    if (getAttrUseInputVectors(map)) {
        const varying Color cu = evalAttrInputU(map, tls, state);
        const varying Color cv = evalAttrInputV(map, tls, state);
        u = Vec3f_ctor(cu.r, cu.g, cu.b);
        v = Vec3f_ctor(cv.r, cv.g, cv.b);
    }

    if (isZero(u) || isZero(v)) {
        return n;
    }

    u = normalize(u);
    v = normalize(v);
    const uniform Vec3f frequencyU = getAttrFrequencyU(map);
    const uniform Vec3f frequencyV = getAttrFrequencyV(map);
    const uniform float amplitudeU = getAttrAmplitudeU(map);
    const uniform float amplitudeV = getAttrAmplitudeV(map);

    varying Vec3f pos;
    uniform SHADING_Space space = (SHADING_Space)getAttrNoiseSpace(map);
    if (space == SHADING_SPACE_TEXTURE) {
        pos = Vec3f_ctor(state.mSt.x,
                         state.mSt.y,
                         0.f);
    } else if (space == SHADING_SPACE_HAIR_SURFACE_ST) {
        varying Vec2f uv = getVec2fAttribute(tls, state, me->mHairSurfaceSTKey);
        pos = Vec3f_ctor(uv.x, uv.y, 0.0f);
    } else if (space == SHADING_SPACE_HAIR_CLOSEST_SURFACE_ST) {
        varying Vec2f uv = getVec2fAttribute(tls, state, me->mHairClosestSurfaceSTKey);
        pos = Vec3f_ctor(uv.x, uv.y, 0.0f);
    } else {
        uniform int inputSourceMode;
        varying Vec3f inputPosition;
        if (space == SHADING_SPACE_REFERENCE) {
            inputSourceMode = INPUT_SOURCE_MODE_REF_P_REF_N;
        } else if (space == SHADING_SPACE_INPUT_COORDINATES) {
            inputSourceMode = INPUT_SOURCE_MODE_ATTR;
            inputPosition = evalAttrInputTextureCoordinate(map, tls, state);
        } else {
            inputSourceMode = INPUT_SOURCE_MODE_P_N;
        }

        Vec3f pos_ddx, pos_ddy, pos_ddz;
        if (!PRIMVAR_getPosition(tls, state,
                                 inputSourceMode,
                                 inputPosition,
                                 (const uniform Xform * uniform)nullptr, // no extra transform
                                 space,
                                 me->mRefPKey,
                                 pos, pos_ddx, pos_ddy, pos_ddz)) {
            // Log missing ref_P data message
            logEvent(map, me->sErrorMissingReferenceData);
            return n;
        }
    }

    // no fractal for now, only smooth noise, [-1, 1] domain
    varying Vec3f dummy; // gets useless derivatives
    const varying float angle = 0.f;
    varying float thetaU = NOISE_simplex3D(me->mNoiseU, frequencyU * pos, angle, dummy);
    varying float thetaV = NOISE_simplex3D(me->mNoiseV, frequencyV * pos, angle, dummy);

    thetaU = max(-1.f, min(1.f, thetaU * amplitudeU)) * 1.57079632f; // pi/2
    thetaV = max(-1.f, min(1.f, thetaV * amplitudeV)) * 1.57079632f; // pi/2
    varying Vec3f result = rotateVector(n, u, thetaU);
    result = rotateVector(result, v, thetaV);

    // clamp to hemisphere
    float ndr = dot(result, n);
    if (ndr < sEpsilon) {
        result = result + (sEpsilon - ndr) * n;
        result = normalize(result);
    }

    return result;
}

DEFINE_NORMALMAP_SHADER(DistortNormalMap, sampleNormal)

