// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file LODMap.cc

#include "attributes.cc"
#include "LODMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>
#include <scene_rdl2/render/util/stdmemory.h>

using namespace scene_rdl2::math;

RDL2_DSO_CLASS_BEGIN(LODMap, scene_rdl2::rdl2::Map)

public:
    LODMap(const scene_rdl2::rdl2::SceneClass& sceneClass,
           const std::string& name);

    ~LODMap() override;
    void update() override;

private:
    static void sample(const scene_rdl2::rdl2::Map* self,
                       moonray::shading::TLState *tls,
                       const moonray::shading::State& state,
                       Color* sample);

    static float getAveragePixelWidth(const moonray::shading::State& state);

    ispc::LODMap mIspcData;

    std::unique_ptr<moonray::shading::Xform> mXform;

RDL2_DSO_CLASS_END(LODMap)



LODMap::LODMap(const scene_rdl2::rdl2::SceneClass& sceneClass,
               const std::string& name):
               Parent(sceneClass, name)
{
    mSampleFunc = LODMap::sample;
    mSampleFuncv = (scene_rdl2::rdl2::SampleFuncv) ispc::LODMap_getSampleFunc();
}

LODMap::~LODMap()
{
}

void
LODMap::update()
{
    // Construct Xform with default transforms for camera and screen.
    mXform = fauxstd::make_unique<moonray::shading::Xform>(this,
                                                           nullptr,
                                                           nullptr,
                                                           nullptr);

    mIspcData.mXform = mXform->getIspcXform();
    mIspcData.mStart = get(attrStart);
    mIspcData.mRange = get(attrStop) - mIspcData.mStart;
    if (mIspcData.mStart <= sEpsilon ||
        mIspcData.mRange <= sEpsilon) {
        fatal("LOD Range Invalid, Ensure start, stop are positive & \"start < stop\".");
    }
}

float
LODMap::getAveragePixelWidth(const moonray::shading::State& state)
{
    return (length(state.getdPdx()) +
            length(state.getdPdy()))*0.5f;
}

void
LODMap::sample(const scene_rdl2::rdl2::Map*     self,
                     moonray::shading::TLState* tls,
               const moonray::shading::State&   state,
                     Color*                     sample)
{
    const LODMap* me = static_cast<const LODMap*>(self);

    float featureValue = 0.0f;

    if (me->get(attrLODType) == ispc::LODMapType::FEATURE_WIDTH) {
        // Get Feature Width
        featureValue = getAveragePixelWidth(state);
    } else {
        // Get Camera Distance
        const Vec3f pos = me->mXform->transformPoint(ispc::SHADING_SPACE_RENDER,
                                                           ispc::SHADING_SPACE_CAMERA,
                                                           state,
                                                           state.getP());

        featureValue = length(pos);
    }

    const float lodValue = saturate((featureValue - me->mIspcData.mStart) / me->mIspcData.mRange);

    const Color colorA = evalColor(me, attrNearValue, tls, state);
    const Color colorB = evalColor(me, attrFarValue, tls, state);

    *sample = lerp(colorA, colorB, lodValue);
}

