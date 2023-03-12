// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//

#pragma once

#include <scene_rdl2/scene/rdl2/Types.h>

namespace moonshine {
namespace interpolation {

float boxStep(const float x, float edge);
float smoothStep(const float x, float edge0, float edge1);
float smootherStep(const float x, float edge0, float edge1);

// This function takes the circular nature of the color wheel into
// account and wraps the hue correctly around the color wheel by
// taking the shortest path around the circle.
// https://www.alanzucconi.com/2016/01/06/colour-interpolation/2/
scene_rdl2::math::Color lerpHSV(float t,
                           const scene_rdl2::math::Color& colorA,
                           const scene_rdl2::math::Color& colorB);

}
}


