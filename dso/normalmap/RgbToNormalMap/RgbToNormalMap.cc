// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.cc"
#include "RgbToNormalMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>

using namespace scene_rdl2::math;

RDL2_DSO_CLASS_BEGIN(RgbToNormalMap, NormalMap)

public:
    RgbToNormalMap(const SceneClass& sceneClass,
                   const std::string& name);

private:
    static void sampleNormal(const NormalMap* self,
                             moonray::shading::TLState* tls,
                             const moonray::shading::State& state,
                             Vec3f* sample);

RDL2_DSO_CLASS_END(RgbToNormalMap)

//----------------------------------------------------------------------------

RgbToNormalMap::RgbToNormalMap(const SceneClass& sceneClass,
                               const std::string& name)
    : Parent(sceneClass, name)
{
    mSampleNormalFunc = RgbToNormalMap::sampleNormal;
    mSampleNormalFuncv = (SampleNormalFuncv)ispc::RgbToNormalMap_getSampleFunc();
}

void
RgbToNormalMap::sampleNormal(const NormalMap* self,
                             moonray::shading::TLState* tls,
                             const moonray::shading::State& state,
                             Vec3f* sample)
{
    const RgbToNormalMap* me = static_cast<const RgbToNormalMap*>(self);
    const Color input = evalColor(me, attrInput, tls, state);
    Vec3f result(input.r, input.g, input.b);
    *sample = result;
}

