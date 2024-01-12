// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file RandomMap.cc

#include "attributes.cc"
#include "RandomMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>
#include <scene_rdl2/render/util/Random.h>

static ispc::StaticRandomMapData sStaticRandomMapData;

using namespace scene_rdl2::math;
using namespace moonray::shading;

//---------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(RandomMap, Map)

public:
    RandomMap(const SceneClass& sceneClass, const std::string& name);
    ~RandomMap();
    virtual void update() override;

private:
    static void sample(const Map* self, moonray::shading::TLState *tls,
                       const moonray::shading::State& state, Color* sample);

    ispc::RandomMap mIspc; // must be first member

RDL2_DSO_CLASS_END(RandomMap)

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

RandomMap::RandomMap(const SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mSampleFunc = RandomMap::sample;
    mSampleFuncv = (SampleFuncv) ispc::RandomMap_getSampleFunc();

    // Build the random table
    scene_rdl2::util::Random rnd;
    for (int i = 0; i < ispc::RANDOM_TABSIZE; ++i) {
        Color c;
        c.r = rnd.getNextFloat();
        c.g = rnd.getNextFloat();
        c.b = rnd.getNextFloat();
        asCpp(sStaticRandomMapData.mRandomTable[i]) = c;
    }

    mIspc.mStaticData = (ispc::StaticRandomMapData *)&sStaticRandomMapData;
}

RandomMap::~RandomMap()
{
}

void
RandomMap::update()
{
}

void
RandomMap::sample(const Map* self, moonray::shading::TLState *tls,
                 const moonray::shading::State& state, Color* sample)
{
    const RandomMap* me = static_cast<const RandomMap*>(self);

    const Color input       = evalColor(me, attrInput, tls, state);
    const int   seed        = me->get(attrSeed);
    const float outMin      = me->get(attrOutMin);
    const float outMax      = me->get(attrOutMax);
    const bool  monochrome  = me->get(attrMonochrome);

    const int TABMASK = ispc::RANDOM_TABSIZE - 1;

    Color result;
    if (monochrome) {
        int idxR = getIndex(input.r, ispc::RANDOM_TABSIZE, seed, TABMASK);
        result = Color(sStaticRandomMapData.mRandomTable[idxR].r,
                       sStaticRandomMapData.mRandomTable[idxR].r,
                       sStaticRandomMapData.mRandomTable[idxR].r);
    } else {
        int idxR = getIndex(input.r, ispc::RANDOM_TABSIZE, seed + 0, TABMASK);
        int idxG = getIndex(input.g, ispc::RANDOM_TABSIZE, seed + 1, TABMASK);
        int idxB = getIndex(input.b, ispc::RANDOM_TABSIZE, seed + 2, TABMASK);
        result = Color(sStaticRandomMapData.mRandomTable[idxR].r,
                       sStaticRandomMapData.mRandomTable[idxG].g,
                       sStaticRandomMapData.mRandomTable[idxB].b);
    }

    result = result * (outMax - outMin) + Color(outMin);

    *sample = result;
}

