// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file RandomNormalMap.cc

#include "attributes.cc"
#include "RandomNormalMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>
#include <scene_rdl2/render/util/Random.h>

static ispc::StaticRandomNormalMapData sStaticRandomNormalMapData;

using namespace scene_rdl2::math;
using namespace moonray::shading;

//---------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(RandomNormalMap, scene_rdl2::rdl2::NormalMap)

public:
    RandomNormalMap(const SceneClass& sceneClass, const std::string& name);
    ~RandomNormalMap();
    virtual void update() override;

private:
    static void sampleNormal(const scene_rdl2::rdl2::NormalMap* self,
                             moonray::shading::TLState *tls,
                             const moonray::shading::State& state,
                             Vec3f* sample);

    ispc::RandomNormalMap mIspc; // must be first member

RDL2_DSO_CLASS_END(RandomNormalMap)

//---------------------------------------------------------------------------

int getIndex(float input, int tabSize, int seed, int tabMask)
{
    // check ceil of input for overflow
    if (static_cast<int>(input) + 1 < (INT_MAX - seed) / tabSize) {
        return (static_cast<int>(input * tabSize) + seed) % tabMask;
    } else {
        float iPart = 0.f;
        const float fPart = modff(input, &iPart);
        // expand ((i + f) * b + seed) % m with distributive to prevent overflow
        // assumed m = b - 1 ==> b % m = 1, see TABMASK = TABSIZE - 1 in sample()
        const int highP = static_cast<int>(iPart) % tabMask;
        return ((highP + static_cast<int>(fPart * tabSize) % tabMask) % tabMask + seed) % tabMask;
    }
}

RandomNormalMap::RandomNormalMap(const SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mSampleNormalFunc = RandomNormalMap::sampleNormal;
    mSampleNormalFuncv = (scene_rdl2::rdl2::SampleNormalFuncv) ispc::RandomNormalMap_getSampleFunc();

    // Build the random table
    scene_rdl2::util::Random rnd;
    for (int i = 0; i < ispc::RANDOM_TABSIZE; ++i) {
        sStaticRandomNormalMapData.mRandomTable[i] = rnd.getNextFloat();
    }

    mIspc.mStaticData = (ispc::StaticRandomNormalMapData *)&sStaticRandomNormalMapData;
}

RandomNormalMap::~RandomNormalMap()
{
}

void
RandomNormalMap::update()
{
    // Get whether or not the normals have been reversed
    mOptionalAttributes.push_back(moonray::shading::StandardAttributes::sReversedNormals);
    mIspc.mReversedNormalsIndx = moonray::shading::StandardAttributes::sReversedNormals.getIndex();
}

void
RandomNormalMap::sampleNormal(const scene_rdl2::rdl2::NormalMap* self,
                              moonray::shading::TLState* tls,
                              const moonray::shading::State& state,
                              Vec3f* sample)
{
    const RandomNormalMap* me = static_cast<const RandomNormalMap*>(self);

    const Color input = evalColor(me, attrInput, tls, state);
    const int seed = me->get(attrSeed);
    const int TABMASK = ispc::RANDOM_TABSIZE - 1;

    const int idx1 = getIndex(input.r,        ispc::RANDOM_TABSIZE, seed, TABMASK);
    const int idx2 = getIndex(input.r * 2.5f, ispc::RANDOM_TABSIZE, seed, TABMASK); // arbitrary mult for hashing
    const float u1 = sStaticRandomNormalMapData.mRandomTable[idx1];
    const float u2 = sStaticRandomNormalMapData.mRandomTable[idx2];

    const float phi = sTwoPi * u2;
    float cp = 0.f;
    float sp = 0.f;
    sincos(phi, &sp, &cp);

    // somewhat arbitrary disk sampling, with z
    // biased to center, avoids horizon which often clips
    Vec3f normal = Vec3f(cp * u1, sp * u1, 1.f);
    normal = normalize(normal);

    // Transform from tangent space to shade space
    bool reversedNormals = false;
    if (state.isProvided(moonray::shading::StandardAttributes::sReversedNormals)) {
        reversedNormals = state.getAttribute(moonray::shading::StandardAttributes::sReversedNormals);
    }
    const scene_rdl2::math::Vec3f statedPds = reversedNormals ? state.getdPds() * -1.0f : state.getdPds() * 1.0f;
    const ReferenceFrame frame(state.getN(), scene_rdl2::math::normalize(statedPds));
    const Vec3f renderN = frame.localToGlobal(normal);

    *sample = renderN;
}

