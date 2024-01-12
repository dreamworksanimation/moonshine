// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//

#include "Interpolation.h"
#include <scene_rdl2/common/math/Math.h>
#include <scene_rdl2/common/math/MathUtil.h>
#include <scene_rdl2/common/math/Color.h>

namespace moonshine {
namespace interpolation {

using namespace scene_rdl2::math;

float
boxStep(const float x, const float edge)
{
    return (x > edge) ? 0.0f : 1.0f;
}

// from http://en.wikipedia.org/wiki/Smoothstep
float
smoothStep(const float x, const float edge0, const float edge1)
{
    const float t = saturate((x - edge0) / scene_rdl2::math::max(sEpsilon, (edge1 - edge0)));
    return t * t * (3 - 2 * t);
}

// Ken Perlin's 'smootherstep' from http://en.wikipedia.org/wiki/Smoothstep
float
smootherStep(const float x, const float edge0, const float edge1)
{
    const float t = saturate((x - edge0) / scene_rdl2::math::max(sEpsilon, (edge1 - edge0)));
    return t * t * t * (t * (t * 6 - 15) + 10);
}

// This function takes the circular nature of the color wheel into
// account and wraps the hue correctly around the color wheel by
// taking the shortest path around the circle.
// https://www.alanzucconi.com/2016/01/06/colour-interpolation/2/
Color
lerpHSV(float t, const Color& colorA, const Color& colorB)
{
    if (t <= 0) return colorA;
    if (t >= 1) return colorB;
    float hue;
    float colorAHue = colorA.r;
    float colorBHue = colorB.r;
    float dist = colorBHue - colorAHue;
    if (colorAHue > colorBHue) {
        colorBHue = colorA.r;
        colorAHue = colorB.r;
        dist *= -1.f;
        t = 1.f - t;
    }

    if (dist > 0.5f) {
        colorAHue = colorAHue + 1.f;
        hue = scene_rdl2::math::fmod(colorAHue + t * (colorBHue - colorAHue), 1.f);
    } else {
        hue = colorAHue + t * dist;
    }

    return Color(hue,
                       colorA.g + t * (colorB.g - colorA.g),
                       colorA.b + t * (colorB.b - colorB.b));
}

}
}


