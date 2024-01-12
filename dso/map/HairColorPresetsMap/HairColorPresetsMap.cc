// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.cc"
#include "HairColorPresetsMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>

using namespace scene_rdl2::math;
using namespace moonray::shading;

//---------------------------------------------------------------------------
// Presets calculated using http://www.collectedwebs.com/art/colors/hair/
// as inspiration.
RDL2_DSO_CLASS_BEGIN(HairColorPresetsMap, scene_rdl2::rdl2::Map)

public:
    HairColorPresetsMap(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name);
    ~HairColorPresetsMap();
    virtual void update();

private:
    static void sample(const scene_rdl2::rdl2::Map* self, moonray::shading::TLState *tls,
                       const moonray::shading::State& state, Color* sample);

RDL2_DSO_CLASS_END(HairColorPresetsMap)

//---------------------------------------------------------------------------

HairColorPresetsMap::HairColorPresetsMap(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mSampleFunc = HairColorPresetsMap::sample;
    mSampleFuncv = (scene_rdl2::rdl2::SampleFuncv) ispc::HairColorPresetsMap_getSampleFunc();
}

HairColorPresetsMap::~HairColorPresetsMap()
{

}

void
HairColorPresetsMap::update()
{
    const int operation = get(attrColor);
    if (operation < 0 || operation > 14) {
        fatal("Unsupported HairColorPreset.");
    }
}

void
HairColorPresetsMap::sample(const scene_rdl2::rdl2::Map* self,
                            moonray::shading::TLState *tls,
                            const moonray::shading::State& state,
                            Color* sample)
{
    const HairColorPresetsMap* me = static_cast<const HairColorPresetsMap*>(self);

    // Get the input parameters
    const int colorChoice  = me->get(attrColor);
    Color result;

    switch(colorChoice) {
        case ispc::HairColor::BLACK:
            result = Rgb(0.0008f, 0.0008f, 0.0007f);
            break;
        case ispc::HairColor::GRAY:
            result = Rgb(0.1669f, 0.1562f, 0.1502f);
            break;
        case ispc::HairColor::PLATINUM_BLOND:
            result = Rgb(0.829f, 0.640525f, 0.418645f);
            break;
        case ispc::HairColor::LIGHT_BLOND:
            result = Rgb(0.5f, 0.392323f, 0.250527f);
            break;
        case ispc::HairColor::GOLDEN_BLOND:
            result = Rgb(0.287f, 0.18693f, 0.09499f);
            break;
        case ispc::HairColor::STRAWBERRY_BLOND:
            result = Rgb(0.50543f, 0.17491f, 0.07663f);
            break;
        case ispc::HairColor::LIGHT_RED:
            result = Rgb(0.6196f, 0.10853f, 0.04877f);
            break;
        case ispc::HairColor::DARK_RED:
            result = Rgb(0.3577f, 0.0866f, 0.0696f);
            break;
        case ispc::HairColor::LIGHT_AUBURN:
            result = Rgb(0.3804f, 0.1175f, 0.056614f);
            break;
        case ispc::HairColor::DARK_AUBURN:
            result = Rgb(0.1115f, 0.056614f, 0.036655f);
            break;
        case ispc::HairColor::BROWN:
            result = Rgb(0.1449f, 0.07382f, 0.05112f);
            break;
        case ispc::HairColor::DARK_BROWN:
            result = Rgb(0.05261f, 0.03341f, 0.01774f);
            break;
        case ispc::HairColor::GOLDEN_BROWN:
            result = Rgb(0.1175f, 0.08513f, 0.0469f);
            break;
        case ispc::HairColor::ASH_BROWN:
            result = Rgb(0.4159f, 0.2555f, 0.157f);
            break;
        case ispc::HairColor::CHESTNUT_BROWN:
            result = Rgb(0.07806f, 0.05459f, 0.05459f);
            break;
        default:
            result[0] = 0.0;
            result[1] = 0.0;
            result[2] = 0.0;
            break;
    }

    *sample = result;
}


//---------------------------------------------------------------------------

