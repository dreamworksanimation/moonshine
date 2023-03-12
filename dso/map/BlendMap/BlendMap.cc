// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file BlendMap.cc

#include "attributes.cc"
#include "BlendMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>

using namespace moonray::shading;
using namespace scene_rdl2::math;

//----------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(BlendMap, scene_rdl2::rdl2::Map)

public:
    BlendMap(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name);
    ~BlendMap() override;
    void update() override;

private:
    static void sample(const scene_rdl2::rdl2::Map *self, moonray::shading::TLState *tls,
                       const moonray::shading::State &state, Color *sample);

    bool verifyInputs();

RDL2_DSO_CLASS_END(BlendMap)

//----------------------------------------------------------------------------

BlendMap::BlendMap(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name):
    Parent(sceneClass, name)
{
    mSampleFunc = BlendMap::sample;
    mSampleFuncv = (scene_rdl2::rdl2::SampleFuncv) ispc::BlendMap_getSampleFunc();
}

BlendMap::~BlendMap()
{
}

bool
BlendMap::verifyInputs() {
    if (get(attrBlendType) > 1 || get(attrBlendType) < 0) {
        fatal("Blend: Unsupported blend type");
        return false;
    }

    if (get(attrThresholdMax) < get(attrThresholdMin)) {
        fatal("Blend: threshold min must be less than threshold max");
        return false;
    }
    return true;
}

void
BlendMap::update()
{
    if (!verifyInputs()) {
        fatal("Blend input validation failed");
        return;
    }
}

void
BlendMap::sample(const scene_rdl2::rdl2::Map *self, moonray::shading::TLState *tls,
                        const moonray::shading::State &state, Color *sample)
{
    const BlendMap* me = static_cast<const BlendMap*>(self);

    const float blendAmount  = evalFloat(me, attrBlendAmount, tls, state);
    const float thresholdMin = evalFloat(me, attrThresholdMin, tls, state);
    const float thresholdMax = evalFloat(me, attrThresholdMax, tls, state);
    const int blendType      = me->get(attrBlendType);

    if (blendAmount <= thresholdMin) {
        *sample = evalColor(me, attrColorA, tls, state);
    } else if (blendAmount >= thresholdMax) {
        *sample = evalColor(me, attrColorB, tls, state);
    } else {
        const Color colorA = evalColor(me, attrColorA, tls, state);
        const Color colorB = evalColor(me, attrColorB, tls, state);
        const float divisor = (thresholdMax - thresholdMin);

        float t = 0.0f;
        if (divisor > 0.0f)
            t = (blendAmount - thresholdMin) * rcp(divisor);

        if (blendType == ispc::CUBIC) {
            t = t * t * (3 - 2 * t);
        }
        *sample = lerp(colorA, colorB, t);
    }
}

