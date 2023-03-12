// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file RgbToLabMap.ispc

// auto-generated from RgbToLabMap.json
#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

struct Lab {
    float l;
    float a;
    float b;
};

struct Xyz {
    float x;
    float y;
    float z;
};

//
// RGB -> XYZ, using sRGB/709 primaries + D65 white
//
static varying Xyz
rgbToXyz(const varying Color &rgb)
{   
    static const uniform float kCoef[9] = {
        0.4124564f,  0.3575761f,  0.1804375f,
        0.2126729f,  0.7151522f,  0.0721750f,
        0.0193339f,  0.1191920f,  0.9503041f
    };

    Xyz xyz;

    xyz.x = kCoef[0]*rgb.r + kCoef[1]*rgb.g + kCoef[2]*rgb.b;
    xyz.y = kCoef[3]*rgb.r + kCoef[4]*rgb.g + kCoef[5]*rgb.b;
    xyz.z = kCoef[6]*rgb.r + kCoef[7]*rgb.g + kCoef[8]*rgb.b;

    return xyz;
}

//
// XYZ -> LAB, using D65 whitepoint
//
static varying  Lab
xyzToLab(const varying Xyz &xyz)
{
    const float kE      = .008856f;
    const float kK      = 903.3f;
    const Xyz  white   = { 0.950456f, 
                           1.000000f,
                           1.089058f }; // D65

    Xyz f;
    f.x = xyz.x / white.x;
    f.y = xyz.y / white.y;
    f.z = xyz.z / white.z;

    if (f.x > kE) {
        f.x = pow(f.x,1.0/3);
    } else {
        f.x = (kK * f.x + 16)/116;
    }

    if (f.y > kE) {
        f.y = pow(f.y,1.0/3); 
    } else {
        f.y = (kK * f.y + 16)/116;
    }

    if (f.z > kE) {
        f.z = pow(f.z,1.0/3);
    } else {
        f.z = (kK * f.z + 16)/116;
    }

    Lab lab;
    lab.l = 116*f.y - 16;
    lab.a = 500*(f.x - f.y);
    lab.b = 200*(f.y - f.z);

    return lab;
}

static Color
sample(const uniform Map *            uniform map,
             uniform ShadingTLState * uniform tls,
       const varying State &                  state)
{
    Color inputColor = evalAttrInputColor(map, tls, state);
   
    // Convert RGB -> XYZ -> LAB
    Lab lab = xyzToLab( rgbToXyz(inputColor) );
    Color s = { lab.l, lab.a, lab.b };
    return s;
}

DEFINE_MAP_SHADER(RgbToLabMap, sample)

