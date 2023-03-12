// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file MultiChannelToFloatMap.cc

/// Converts Color input to a float using the following modes:
/// R: the R component
/// G: the G component
/// B: the B component
/// C: the Cyan basis vector    (0, 1, 1)
/// M: the Magenta basis vector (1, 0, 1)
/// Y: the Yellow basis vector  (1, 1, 0)
/// W: the White basis vector   (1, 1, 1)
/// The output is a Color that its RGB value is filled by the converted float

#include "attributes.cc"
#include "MultiChannelToFloatMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>

using namespace scene_rdl2::math;
using namespace scene_rdl2::rdl2;

RDL2_DSO_CLASS_BEGIN(MultiChannelToFloatMap, Map)
public:
    MultiChannelToFloatMap(SceneClass const &sceneClass, std::string const &name);
    void update() override;

private:
    static void sample(const Map *self, moonray::shading::TLState *tls,
                       const moonray::shading::State &state, Color *sample);

    ispc::MultiChannelToFloatMap mIspc; // must be the 1st member

RDL2_DSO_CLASS_END(MultiChannelToFloatMap)

namespace {

float
dotColor(const Color& a,
         const Color& b)
{
    return a.r * b.r + a.g * b.g + a.b * b.b;
}

float
lengthSqColor(const Color& input)
{
    return dotColor(input, input);
}

ispc::MultiChannelToFloatMode
categorizeColor(const Color& input, Color& result)
{
    result = sBlack;

    if (isEqual(input, sBlack)) {
        return ispc::MultiChannelToFloatMode::K;
    }

    static const Color red =       Color(1.f, 0.f, 0.f);
    static const Color green =     Color(0.f, 1.f, 0.f);
    static const Color blue =      Color(0.f, 0.f, 1.f);
    static const Color cyan =      Color(0.f, 1.f, 1.f);
    static const Color magenta =   Color(1.f, 0.f, 1.f);
    static const Color yellow =    Color(1.f, 1.f, 0.f);

    // Calculate the length of the vector rejection from the input color to each
    // of the 7 color axes.
    const Color projOnRed     = dotColor(input, red) * red;
    const Color projOnGreen   = dotColor(input, green) * green;
    const Color projOnBlue    = dotColor(input, blue) * blue;
    const Color projOnCyan    = (dotColor(input, cyan) / 2.f) * cyan;
    const Color projOnMagenta = (dotColor(input, magenta) / 2.f) * magenta;
    const Color projOnYellow  = (dotColor(input, yellow) / 2.f) * yellow;
    const Color projOnWhite   = (dotColor(input, sWhite) / 3.f) * sWhite;
    const float distSqFromRed     = lengthSqColor(input - projOnRed);
    const float distSqFromGreen   = lengthSqColor(input - projOnGreen);
    const float distSqFromBlue    = lengthSqColor(input - projOnBlue);
    const float distSqFromCyan    = lengthSqColor(input - projOnCyan);
    const float distSqFromMagenta = lengthSqColor(input - projOnMagenta);
    const float distSqFromYellow  = lengthSqColor(input - projOnYellow);
    const float distSqFromWhite   = lengthSqColor(input - projOnWhite);

    // Generate weight based on linear combination of distance to the two closest
    // color basis vectors.
    const float distances[7] = {distSqFromRed, distSqFromGreen, distSqFromBlue,
            distSqFromCyan, distSqFromMagenta, distSqFromYellow, distSqFromWhite};

    float min = 1.f;
    float secondMin = 1.f;
    for (unsigned int i = 0; i < 7; ++i) {
        if (distances[i] <= min) {
            secondMin = min;
            min = distances[i];
        } else if (distances[i] < secondMin) {
            secondMin = distances[i];
        }
    }

    // Use inverse weight. The closer to the primary color axis (min) and the further
    // from the secondary color axis (secondMin) the greater the weight.
    //     If min == 0, weight == 1
    //     Lowest possible weight should be 0.5 when min == secondMin
    if (isZero(min + secondMin)) {
        // zero vector
        return ispc::MultiChannelToFloatMode::K;
    }
    // Technically should take square root since these values are lengths squared
    // but in practice there is almost no difference
    const float weight = secondMin / (min + secondMin);

    // Return the color axis that is closest to the input color
    // Set result to the one of the non-zero components of the weighted projection
    if (isEqual(min, distSqFromRed)) {
        result = Color((weight * projOnRed).r);
        return ispc::MultiChannelToFloatMode::R;
    } else if (isEqual(min, distSqFromGreen)) {
        result = Color((weight * projOnGreen).g);
        return ispc::MultiChannelToFloatMode::G;
    } else if (isEqual(min, distSqFromBlue)) {
        result = Color((weight * projOnBlue).b);
        return ispc::MultiChannelToFloatMode::B;
    } else if (isEqual(min, distSqFromCyan)) {
        result = Color((weight * projOnCyan).g);
        return ispc::MultiChannelToFloatMode::C;
    } else if (isEqual(min, distSqFromMagenta)) {
        result = Color((weight * projOnMagenta).r);
        return ispc::MultiChannelToFloatMode::M;
    } else if (isEqual(min, distSqFromYellow)) {
        result = Color((weight * projOnYellow).r);
        return ispc::MultiChannelToFloatMode::Y;
    } else if (isEqual(min, distSqFromWhite)) {
        result = Color((weight * projOnWhite).r);
        return ispc::MultiChannelToFloatMode::W;
    } else {
        return ispc::MultiChannelToFloatMode::K;
    }
}

} // anonymous namespace

MultiChannelToFloatMap::MultiChannelToFloatMap(const SceneClass& sceneClass,
                                     const std::string& name) :
    Parent(sceneClass, name)
{
    mSampleFunc = MultiChannelToFloatMap::sample;
    mSampleFuncv = (SampleFuncv) ispc::MultiChannelToFloatMap_getSampleFunc();
}

void
MultiChannelToFloatMap::update()
{
    if (hasChanged(attrMode)) {
        mIspc.mMode = get(attrMode);
    }
}

void
MultiChannelToFloatMap::sample(const Map* self,
                          moonray::shading::TLState *tls,
                          const moonray::shading::State& state,
                          Color* sample)
{
    const MultiChannelToFloatMap* me = static_cast<const MultiChannelToFloatMap*>(self);
    const Color input = clamp(evalColor(me, attrInput, tls, state));
    const float tolerance = clamp(evalFloat(me, attrTolerance, tls, state));

    Color result;
    const ispc::MultiChannelToFloatMode colorCategory = categorizeColor(input, result);

    *sample = sBlack;
    if ((1.f - result.r) > tolerance) {
        return;
    }

    switch (me->mIspc.mMode) {
    case ispc::MultiChannelToFloatMode::R:
    {
        if (colorCategory == ispc::MultiChannelToFloatMode::R) {
            *sample = result;
        }
        break;
    }
    case ispc::MultiChannelToFloatMode::G:
    {
        if (colorCategory == ispc::MultiChannelToFloatMode::G) {
            *sample = result;
        }
        break;
    }
    case ispc::MultiChannelToFloatMode::B:
    {
        if (colorCategory == ispc::MultiChannelToFloatMode::B) {
            *sample = result;
        }
        break;
    }
    case ispc::MultiChannelToFloatMode::C:
    {
        if (colorCategory == ispc::MultiChannelToFloatMode::C) {
            *sample = result;
        }
        break;
    }
    case ispc::MultiChannelToFloatMode::M:
    {
        if (colorCategory == ispc::MultiChannelToFloatMode::M) {
            *sample = result;
        }
        break;
    }
    case ispc::MultiChannelToFloatMode::Y:
    {
        if (colorCategory == ispc::MultiChannelToFloatMode::Y) {
            *sample = result;
        }
        break;
    }
    case ispc::MultiChannelToFloatMode::W:
    {
        if (colorCategory == ispc::MultiChannelToFloatMode::W) {
            *sample = result;
        }
        break;
    }
    default:
        break;
    }
    return;
}

