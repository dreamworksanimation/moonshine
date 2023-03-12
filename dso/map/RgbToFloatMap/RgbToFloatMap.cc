// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file RgbToFloatMap.cc

/// Converts Color input to a float using the following modes:
/// R: the R component
/// G: the G component
/// B: the B component
/// Min: Minimum component.
/// Max: Maximum component.
/// Average: Average of the RGB components.
/// Sum: Sum of the RGB components.
/// Luminance: Perceptual greyscale value as defined by Rec.709.
/// The output is a Color that its RGB value is filled by the converted float

#include "attributes.cc"
#include "RgbToFloatMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>

using namespace scene_rdl2::math;
using namespace scene_rdl2::rdl2;

RDL2_DSO_CLASS_BEGIN(RgbToFloatMap, Map)
public:
    RgbToFloatMap(SceneClass const &sceneClass, std::string const &name);
    void update();

private:
    static void sample(const Map *self, moonray::shading::TLState *tls,
                       const moonray::shading::State &state, Color *sample);

    ispc::RgbToFloatMap mIspc; // must be the 1st member

RDL2_DSO_CLASS_END(RgbToFloatMap)

RgbToFloatMap::RgbToFloatMap(const SceneClass& sceneClass,
        const std::string& name) :
    Parent(sceneClass, name)
{
    mSampleFunc = RgbToFloatMap::sample;
    mSampleFuncv = (SampleFuncv) ispc::RgbToFloatMap_getSampleFunc();
}

void
RgbToFloatMap::update()
{
    if (hasChanged(attrMode)) {
        mIspc.mMode = get(attrMode);
    }
}

void
RgbToFloatMap::sample(const Map* self, moonray::shading::TLState *tls,
                 const moonray::shading::State& state, Color* sample)
{
    const RgbToFloatMap* me = static_cast<const RgbToFloatMap*>(self);
    Color input = evalColor(me, attrInput, tls, state);
    switch (me->mIspc.mMode) {
    case ispc::RgbToFloatMode::R:
        *sample = Color(input.r);
        break;
    case ispc::RgbToFloatMode::G:
        *sample = Color(input.g);
        break;
    case ispc::RgbToFloatMode::B:
        *sample = Color(input.b);
        break;
    case ispc::RgbToFloatMode::MIN:
        *sample = Color(scene_rdl2::math::min(scene_rdl2::math::min(input.r, input.g), input.b));
        break;
    case ispc::RgbToFloatMode::MAX:
        *sample = Color(scene_rdl2::math::max(scene_rdl2::math::max(input.r, input.g), input.b));
        break;
    case ispc::RgbToFloatMode::AVERAGE:
        *sample = Color((input.r + input.g + input.b) / 3.0f);
        break;
    case ispc::RgbToFloatMode::SUM:
        *sample = Color(input.r + input.g + input.b);
        break;
    case ispc::RgbToFloatMode::LUMINANCE:
        *sample = Color(luminance(input));
        break;
    default:
        break;
    }
    return;
}

