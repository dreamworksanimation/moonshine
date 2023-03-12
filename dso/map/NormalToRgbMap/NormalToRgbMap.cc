// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.cc"
#include "NormalToRgbMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>

using namespace scene_rdl2::math;

RDL2_DSO_CLASS_BEGIN(NormalToRgbMap, Map)

public:
    NormalToRgbMap(const SceneClass& sceneClass,
                   const std::string& name);
    void update();

private:
    static void sample(const Map* self,
                       moonray::shading::TLState* tls,
                       const moonray::shading::State& state,
                       Color* sample);

    ispc::NormalToRgbMap mIspc;

RDL2_DSO_CLASS_END(NormalToRgbMap)

//----------------------------------------------------------------------------

NormalToRgbMap::NormalToRgbMap(const SceneClass& sceneClass,
                               const std::string& name)
    : Parent(sceneClass, name)
{
    mSampleFunc = NormalToRgbMap::sample;
    mSampleFuncv = (SampleFuncv)ispc::NormalToRgbMap_getSampleFunc();
}

void
NormalToRgbMap::update()
{
    mIspc.mNormalMap = get(attrInput) ?
                 get(attrInput)->asA<NormalMap>() :
                 nullptr;

    mIspc.mSampleNormalFunc = (mIspc.mNormalMap != nullptr) ?
                              (intptr_t) mIspc.mNormalMap->mSampleNormalFuncv :
                              (intptr_t) nullptr;

    if (mIspc.mNormalMap == nullptr) {
        warn("NormalToRgbMap: NormalMap object not provided as input.");
        return;
    }
}

void
NormalToRgbMap::sample(const Map* self,
                       moonray::shading::TLState* tls,
                       const moonray::shading::State& state,
                       Color* sample)
{
    const NormalToRgbMap* me = static_cast<const NormalToRgbMap*>(self);
    Vec3f input(0.0f);
    if (me->mIspc.mNormalMap != nullptr) {
        me->mIspc.mNormalMap->sampleNormal(tls, state, &input);
    }
    *sample = Color(input.x, input.y, input.z);
}

