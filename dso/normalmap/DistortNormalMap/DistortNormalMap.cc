// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.cc"
#include "DistortNormalMap_ispc_stubs.h"

#include <moonshine/common/noise/Simplex.h>
#include <moonray/map/primvar/Primvar.h>

#include <moonray/common/mcrt_macros/moonray_static_check.h>
#include <moonray/rendering/shading/MapApi.h>

#include <scene_rdl2/render/util/stdmemory.h>

using namespace scene_rdl2::math;

RDL2_DSO_CLASS_BEGIN(DistortNormalMap, NormalMap)

public:
    DistortNormalMap(const SceneClass& sceneClass,
                     const std::string& name);
    void update();

private:
    static void sampleNormal(const NormalMap* self,
                             moonray::shading::TLState* tls,
                             const moonray::shading::State& state,
                             Vec3f* sample);

    ispc::DistortNormalMap mIspc;
    std::unique_ptr<moonshine::noise::Simplex> mNoiseU;
    std::unique_ptr<moonshine::noise::Simplex> mNoiseV;
    NormalMap* mNormalMap;

RDL2_DSO_CLASS_END(DistortNormalMap)

//----------------------------------------------------------------------------

DistortNormalMap::DistortNormalMap(const SceneClass& sceneClass,
                                   const std::string& name)
    : Parent(sceneClass, name)
{
    mSampleNormalFunc = DistortNormalMap::sampleNormal;
    mSampleNormalFuncv = (SampleNormalFuncv)ispc::DistortNormalMap_getSampleFunc();
    mIspc.mRefPKey = moonray::shading::StandardAttributes::sRefP;
    mIspc.mHairSurfaceSTKey = moonray::shading::StandardAttributes::sSurfaceST;
    mIspc.mHairClosestSurfaceSTKey = moonray::shading::StandardAttributes::sClosestSurfaceST;
}

void
DistortNormalMap::update()
{
    if (hasChanged(attrNoiseSpace)) {
        mRequiredAttributes.clear();
        mOptionalAttributes.clear();
        if (get(attrNoiseSpace) == ispc::SHADING_SPACE_REFERENCE) {
            mRequiredAttributes.push_back(mIspc.mRefPKey);
        }
        if (get(attrNoiseSpace) == ispc::SHADING_SPACE_HAIR_SURFACE_ST) {
            mRequiredAttributes.push_back(mIspc.mHairSurfaceSTKey);
        }
        if (get(attrNoiseSpace) == ispc::SHADING_SPACE_HAIR_CLOSEST_SURFACE_ST) {
            mRequiredAttributes.push_back(mIspc.mHairClosestSurfaceSTKey);
        }
    }

    if (hasChanged(attrSeed)) {
        // noise is not using 4D (0)
        mNoiseU = fauxstd::make_unique<moonshine::noise::Simplex>(get(attrSeed), 0);
        mNoiseV = fauxstd::make_unique<moonshine::noise::Simplex>(get(attrSeed) + 2, 0);
        mIspc.mNoiseU = mNoiseU->getIspcSimplex();
        mIspc.mNoiseV = mNoiseV->getIspcSimplex();
    }

    mNormalMap = get(attrInputNormals) ?
                 get(attrInputNormals)->asA<NormalMap>() :
                 nullptr;
    mIspc.mNormalMap = mNormalMap;
    mIspc.mSampleNormalFunc = (mNormalMap != nullptr) ?
                              (intptr_t) mNormalMap->mSampleNormalFuncv :
                              (intptr_t) nullptr;
}

// Rodrigues' rotation formula, assume axis is normalized
Vec3f rotateVector(const Vec3f& v, const Vec3f& axis, float theta) {
    float ct, st;
    sincos(theta, &st, &ct);
    return ct * v + st * cross(axis, v) + dot(axis, v) * (1.f - ct) * axis;
}

void
DistortNormalMap::sampleNormal(const NormalMap* self,
                               moonray::shading::TLState* tls,
                               const moonray::shading::State& state,
                               Vec3f* sample)
{
    const DistortNormalMap* me = static_cast<const DistortNormalMap*>(self);

    Vec3f n = state.getN();
    if (me->mNormalMap != nullptr) {
        me->mNormalMap->sampleNormal(tls, state, &n);
    }

    Vec3f u = state.getdPds();
    Vec3f v = state.getdPdt();

    if (me->get(attrUseInputVectors)) {
        const Color cu = evalColor(me, attrInputU, tls, state);
        const Color cv = evalColor(me, attrInputV, tls, state);
        u = Vec3f(cu.r, cu.g, cu.b);
        v = Vec3f(cv.r, cv.g, cv.b);
    }

    if (isZero(u) || isZero(v)) {
        *sample = n;
        return;
    }

    u = normalize(u);
    v = normalize(v);
    Vec3f frequencyU = me->get(attrFrequencyU);
    Vec3f frequencyV = me->get(attrFrequencyV);
    float amplitudeU = me->get(attrAmplitudeU);
    float amplitudeV = me->get(attrAmplitudeV);

    Vec3f pos;
    ispc::SHADING_Space space = static_cast<ispc::SHADING_Space>(me->get(attrNoiseSpace));
    if (space == ispc::SHADING_SPACE_TEXTURE) {
        pos = Vec3f(state.getSt().x, state.getSt().y, 0.0f);
    } else if (space == ispc::SHADING_SPACE_HAIR_SURFACE_ST) {
        Vec2f uv = state.getAttribute(moonray::shading::StandardAttributes::sSurfaceST);
        pos = Vec3f(uv.x, uv.y, 0.0f);
    } else if (space == ispc::SHADING_SPACE_HAIR_CLOSEST_SURFACE_ST) {
        Vec2f uv =
                state.getAttribute(moonray::shading::StandardAttributes::sClosestSurfaceST);
        pos = Vec3f(uv.x, uv.y, 0.0f);
    } else {
        ispc::PRIMVAR_Input_Source_Mode inputSourceMode;
        Vec3f inputPosition;
        if (space == ispc::SHADING_SPACE_REFERENCE) {
            inputSourceMode = ispc::INPUT_SOURCE_MODE_REF_P_REF_N;
        } else if (space == ispc::SHADING_SPACE_INPUT_COORDINATES) {
            inputSourceMode = ispc::INPUT_SOURCE_MODE_ATTR;
            inputPosition = evalVec3f(me, attrInputTextureCoordinate, tls, state);
        } else {
            inputSourceMode = ispc::INPUT_SOURCE_MODE_P_N;
        }

        Vec3f pos_ddx, pos_ddy, pos_ddz;
        if (!moonshine::primvar::getPosition(tls, state,
                                             inputSourceMode,
                                             inputPosition,
                                             nullptr, // no extra transform
                                             space,
                                             me->mIspc.mRefPKey,
                                             pos, pos_ddx, pos_ddy, pos_ddz)) {
            // Log missing ref_P data message
            moonray::shading::logEvent(me, tls, me->mIspc.sErrorMissingReferenceData);
            *sample = n;
            return;
        }
    }

    // no fractal for now, only smooth noise, [-1, 1] domain
    Vec3f dummy; // gets useless derivatives
    float thetaU = me->mNoiseU->simplex3D(frequencyU * pos, 0.f, dummy);
    float thetaV = me->mNoiseU->simplex3D(frequencyV * pos, 0.f, dummy);

    thetaU = std::max(-1.f, std::min(1.f, thetaU * amplitudeU)) * 1.57079632f; // pi/2
    thetaV = std::max(-1.f, std::min(1.f, thetaV * amplitudeV)) * 1.57079632f; // pi/2
    Vec3f result = rotateVector(n, u, thetaU);
    result = rotateVector(result, v, thetaV);

    // clamp to hemisphere
    float ndr = dot(result, n);
    if (ndr < sEpsilon) {
        result = result + (sEpsilon - ndr) * n;
        result = normalize(result);
    }

    *sample = result;
}

