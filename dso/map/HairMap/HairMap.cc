// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.cc"
#include "HairMap_ispc_stubs.h"

#include <moonray/common/mcrt_macros/moonray_static_check.h>
#include <moonray/rendering/shading/MapApi.h>

#include <string>

using namespace scene_rdl2::math;
using namespace moonray::shading;

//---------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(HairMap, Map)

public:
    HairMap(const SceneClass &sceneClass, const std::string &name);
    ~HairMap();
    virtual void update();

private:
    static void sample(const Map* self, moonray::shading::TLState *tls,
                       const moonray::shading::State& state, Color* sample);

RDL2_DSO_CLASS_END(HairMap)


//---------------------------------------------------------------------------

HairMap::HairMap(const SceneClass &sceneClass, const std::string &name) :
    Parent(sceneClass, name)
{
    mSampleFunc = HairMap::sample;
    mSampleFuncv = (SampleFuncv) ispc::HairMap_getSampleFunc();
}

HairMap::~HairMap()
{
    // do nothing
}

void
HairMap::update()
{
}

void
HairMap::sample(const Map* self, moonray::shading::TLState *tls,
                const moonray::shading::State& state, Color* sample)
{
    const HairMap* me = static_cast<const HairMap*>(self);

    Vec2f st = state.getSt();
    float s = st[0];

    // Bias: we use 1.0 - bias so small bias values push colors towards root
    float bias = evalFloat(me, attrLengthWiseBias, tls, state);
    s = scene_rdl2::math::bias(s, 1.0f - bias);

    // Gain
    float g = evalFloat(me, attrLengthWiseGain, tls, state);
    s = scene_rdl2::math::gain(s, g);

    // length wise color interpolation
    Color baseColor = moonray::shading::evalColor(me, attrBaseHairColor, tls, state);
    Color tipColor = moonray::shading::evalColor(me, attrTipHairColor, tls, state);
    Color columnUvColor = moonray::shading::evalColor(me, attrHairColumnUvColor, tls, state);

    *sample = (baseColor * (1.0f - s) + tipColor * s) * columnUvColor;
}


//---------------------------------------------------------------------------

