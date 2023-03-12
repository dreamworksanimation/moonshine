// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

///
/// @brief Convert an RGB color to LAB
///

// auto-generated from RgbToLabMap.json
#include "attributes.cc"
#include "RgbToLabMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>

#include <cmath>

using namespace moonray;
using namespace scene_rdl2::math;

namespace {

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
    Xyz
    rgbToXyz(const Color &rgb)
    {   
        static const float kCoef[9] = {
            0.4124564,  0.3575761,  0.1804375,
            0.2126729,  0.7151522,  0.0721750,
            0.0193339,  0.1191920,  0.9503041
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
    Lab
    xyzToLab(const Xyz &xyz)
    {
        static const float kE      = .008856f;
        static const float kK      = 903.3f;
        static const Xyz   white{0.950456f, 1.000000f, 1.089058f}; // D65

        Xyz f;
        f.x = xyz.x / white.x;
        f.y = xyz.y / white.y;
        f.z = xyz.z / white.z;

        if (f.x > kE) {
            f.x = std::cbrt(f.x);
        } else {
            f.x = (kK * f.x + 16)/116;
        }

        if (f.y > kE) {
            f.y = std::cbrt(f.y);
        } else {
            f.y = (kK * f.y + 16)/116;
        }

        if (f.z > kE) {
            f.z = std::cbrt(f.z);
        } else {
            f.z = (kK * f.z + 16)/116;
        }

        Lab lab;
        lab.l = 116*f.y - 16;
        lab.a = 500*(f.x - f.y);
        lab.b = 200*(f.y - f.z);

        return lab;
    }

} // namespace


//---------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(RgbToLabMap, scene_rdl2::rdl2::Map)

public:
    RgbToLabMap(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name);
    ~RgbToLabMap();
    virtual void update();

private:
    static void sample(const scene_rdl2::rdl2::Map* self, moonray::shading::TLState *tls,
                       const moonray::shading::State& state, Color* sample);
    
RDL2_DSO_CLASS_END(RgbToLabMap)


//---------------------------------------------------------------------------

RgbToLabMap::RgbToLabMap(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name) :
    Parent(sceneClass, name)
{
    // set the sample function
    mSampleFunc = RgbToLabMap::sample;

    // set the ISPC vectorized sample functions
    // these are only used if ISPC shaders are in use, which
    // currently requires a custom build of moonray.
    // see pbr/integrator/PathIntegrator.cc "TEST_ISPC_SHADERS"
    mSampleFuncv   = (scene_rdl2::rdl2::SampleFuncv) ispc::RgbToLabMap_getSampleFunc();
}

RgbToLabMap::~RgbToLabMap()
{
}


//
// update() is called once per frame when scene scene data has changed.
//
void
RgbToLabMap::update()
{
}

//
// sample() is called every sample point.  The job of this function
// is to return a color value at the location specified by state.
//
void
RgbToLabMap::sample(const scene_rdl2::rdl2::Map* self, moonray::shading::TLState *tls,
                   const moonray::shading::State& state, Color* sample)
{
    const RgbToLabMap* me = static_cast<const RgbToLabMap*>(self);

    Color inputColor = moonray::shading::evalColor(me, attrInputColor, tls, state);
   
    // Convert RGB -> XYZ -> LAB
    Lab lab = xyzToLab( rgbToXyz(inputColor) );
    sample->r = lab.l;
    sample->g = lab.a;
    sample->b = lab.b;
}


//---------------------------------------------------------------------------

