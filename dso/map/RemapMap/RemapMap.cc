// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file RemapMap.cc

#include "attributes.cc"
#include "RemapMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>

using namespace moonray::shading;
using namespace scene_rdl2::math;

//---------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(RemapMap, scene_rdl2::rdl2::Map)

public:
    RemapMap(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name);
    ~RemapMap();
    virtual void update();

private:
    static void sample(const scene_rdl2::rdl2::Map* self, moonray::shading::TLState *tls,
                       const moonray::shading::State& state, Color* sample);

RDL2_DSO_CLASS_END(RemapMap)


namespace {

Color
remapRGB(const Color& input,
         const Color& inMin, const Color& inMax,
         const Color& outMin, const Color& outMax,
         const Color& biasAmount)
{
    const Color inRange = inMax - inMin;
    const Color outRange = outMax - outMin;

    // Handle certain edge cases.  It is slightly unclear what the behavior
    // should be in these cases, but we need to handle them somehow.
    if (isBlack(outRange)) {
        // both outMin and outMax are the same, so all input values remap to the
        // same output value (outMin)
        return outMin;
    } else if (isBlack(inRange)) {
        // both inMin and inMax are the same, so all input values remap to the same
        // output value.  Start off with average of outMin and outMax, bias if necessary.
        Color t = Color(0.5f);
        t.r = bias(t.r, biasAmount.r);
        t.g = bias(t.g, biasAmount.g);
        t.b = bias(t.b, biasAmount.b);

        return t * outRange + outMin;
    }

    Color result = input;
    result = (result - inMin) * rcp(inRange);
    result.r = bias(result.r, biasAmount.r);
    result.g = bias(result.g, biasAmount.g);
    result.b = bias(result.b, biasAmount.b);
    result = result * outRange + outMin;
    return result;
}

Color
remapUniform(const Color& input,
             const float inMin, const float inMax,
             const float outMin, const float outMax,
             const bool applyBias,
             const float biasAmount)
{
    const float inRange = inMax - inMin;
    const float outRange = outMax - outMin;

    // Handle certain edge cases.  It is slightly unclear what the behavior
    // should be in these cases, but we need to handle them somehow.
    if (isZero(outRange)) {
        // both outMin and outMax are the same, so all input values remap to the
        // same output value (outMin)
        return Color(outMin);
    } else if (isZero(inRange)) {
        // both inMin and inMax are the same, so all input values remap to the same
        // output value.  Start off with average of outMin and outMax, bias if necessary.
        float t = 0.5f;
        if (applyBias) {
            t = bias(t, biasAmount);
        }
        return Color(t * outRange + outMin);
    }

    Color result = input;
    for (unsigned int i = 0; i < 3; ++i) {
        float t = (result[i]-inMin) * rcp(inRange);

        if (applyBias) {
            t = bias(t, biasAmount);
        }

        result[i] = t * outRange + outMin;
    }

    return result;
}

} // anonymous namespace

//---------------------------------------------------------------------------

RemapMap::RemapMap(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mSampleFunc = RemapMap::sample;
    mSampleFuncv = (scene_rdl2::rdl2::SampleFuncv) ispc::RemapMap_getSampleFunc();
}

RemapMap::~RemapMap()
{
}

void
RemapMap::update()
{
}

void
RemapMap::sample(const scene_rdl2::rdl2::Map* self, moonray::shading::TLState *tls,
                 const moonray::shading::State& state, Color* sample)
{
    const RemapMap* me = static_cast<const RemapMap*>(self);

    const Color input       = evalColor(me, attrInput, tls, state);
    const float inMin       = me->get(attrInMin);
    const float inMax       = me->get(attrInMax);
    const float outMin      = me->get(attrOutMin);
    const float outMax      = me->get(attrOutMax);
    const float biasAmount  = me->get(attrBias);

    const Color inMinRGB       = me->get(attrInMinRGB);
    const Color inMaxRGB       = me->get(attrInMaxRGB);
    const Color outMinRGB      = me->get(attrOutMinRGB);
    const Color outMaxRGB      = me->get(attrOutMaxRGB);
    const Color biasAmountRGB  = me->get(attrBiasRGB);

    const bool applyBias = !isEqual(biasAmount, 0.5f);

    Color result(input);

    switch(me->get(attrRemapMethod)) {
    case ispc::REMAP_UNIFORM:
        if (!isEqual(inMin, outMin) || !isEqual(inMax, outMax)) {
            // apply remapping and potentially bias
            result = remapUniform(result, inMin, inMax, outMin, outMax, applyBias, biasAmount);
        } else if (applyBias) {
            // apply bias only
            for (unsigned int i = 0; i < 3; ++i) {
                result[i] = bias(result[i], biasAmount);
            }
        }

        if (me->get(attrClamp)) {
            for (unsigned int i = 0; i < 3; ++i) {
                 result[i] = clamp(result[i], me->get(attrClampMin), me->get(attrClampMax));
            }
        }
        break;
    case ispc::REMAP_RGB:
        if (!isEqual(inMinRGB, outMinRGB) || !isEqual(inMaxRGB, outMaxRGB)) {
            // apply remapping and potentially bias
            result = remapRGB(result, inMinRGB, inMaxRGB, outMinRGB, outMaxRGB, biasAmountRGB);
        } else {
            // apply bias only
            for (unsigned int i = 0; i < 3; ++i) {
                result[i] = bias(result[i], biasAmountRGB[i]);
            }
        }
        
        if (me->get(attrClampRGB)) {
            const Color clampMinRGB = me->get(attrClampMinRGB);
            const Color clampMaxRGB = me->get(attrClampMaxRGB);
            result = max(clampMinRGB, min(clampMaxRGB, result));
        }
    
        break;
    default:
        break;
    }

    *sample = result;
}


//---------------------------------------------------------------------------

