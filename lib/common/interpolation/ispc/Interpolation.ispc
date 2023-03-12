// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "Interpolation.isph"

varying float
INTERPOLATION_boxStep(const varying float x,
                      const varying float edge)
{
    return ((x > edge) ? 0.0f : 1.0f);
}

// from http://en.wikipedia.org/wiki/Smoothstep
varying float
INTERPOLATION_smoothStep(const varying float x,
                         const varying float edge0,
                         const varying float edge1)
{
    const varying float t = saturate((x - edge0) / max((edge1 - edge0), sEpsilon));
    return t * t * (3 - 2 * t);
}

// Ken Perlin's 'smootherstep' from http://en.wikipedia.org/wiki/Smoothstep
varying float
INTERPOLATION_smootherStep(const varying float x,
                           const varying float edge0,
                           const varying float edge1)
{
    const varying float t = saturate((x - edge0) / max((edge1 - edge0), sEpsilon));
    return t * t * t * (t * (t * 6 - 15) + 10);
}

// This function takes the circular nature of the color wheel into
// account and wraps the hue correctly around the color wheel by
// taking the shortest path around the circle.
// https://www.alanzucconi.com/2016/01/06/colour-interpolation/2/
varying Color
INTERPOLATION_lerpHSV(varying float t,
                      const varying Color& colorA,
                      const varying Color& colorB)
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
        hue = fmod(colorAHue + t * (colorBHue - colorAHue), 1.f);
    } else {
        hue = colorAHue + t * dist;
    }

    return Color_ctor(hue,
                      colorA.g + t * (colorB.g - colorA.g),
                      colorA.b + t * (colorB.b - colorB.b));
}

