// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

///
/// @file HairMaterial_v3.cc
/// $Id$
///
//

#include "attributes.cc"
#include "HairMaterial_v3_ispc_stubs.h"
#include "labels.h"

#include <moonshine/material/dwabase/DwaBase.h>
#include <moonshine/material/dwabase/DwaBaseLayerable.h>
#include <moonray/rendering/shading/MaterialApi.h>

using namespace scene_rdl2::math;
using namespace scene_rdl2::shading;
using namespace moonshine::dwabase;

namespace {

DECLARE_DWA_BASE_LABELS()

DwaBaseAttributeKeys
collectAttributeKeys()
{
    DwaBaseAttributeKeys keys;

    keys.mCastsCaustics = attrCastsCaustics;
    keys.mPresence = attrPresence;
    keys.mShowEmission = attrShowEmission;
    keys.mEmission = attrEmission;

    keys.mHairColor = attrHairColor;
    keys.mRefractiveIndex = attrRefractiveIndex;
    keys.mHairFresnelType = attrFresnelType;
    keys.mHairCuticleLayerThickness = attrCuticleLayerThickness;
    keys.mHairShowR = attrShowR;
    keys.mHairROffset = attrROffset;
    keys.mHairRRoughness = attrRRoughness;
    keys.mHairRTintColor = attrRTintColor;
    keys.mHairShowTRT = attrShowTRT;
    keys.mHairTRTOffset = attrTRTOffset;
    keys.mHairTRTUseRoughness = attrUseTRTRoughness;
    keys.mHairTRTRoughness = attrTRTRoughness;
    keys.mHairTRTTintColor = attrTRTTintColor;
    keys.mHairShowGlint = attrShowHairGlint;
    keys.mHairGlintRoughness = attrGlintRoughness;
    keys.mHairGlintMinTwists = attrGlintMinTwists;
    keys.mHairGlintMaxTwists = attrGlintMaxTwists;
    keys.mHairGlintEccentricity = attrGlintEccentricity;
    keys.mHairGlintSaturation = attrGlintSaturation;
    keys.mHairShowTT = attrShowTT;
    keys.mHairTTOffset = attrTTOffset;
    keys.mHairTTUseRoughness = attrUseTTRoughness;
    keys.mHairTTRoughness = attrTTRoughness;
    keys.mHairTTSaturation = attrTTSaturation;
    keys.mHairTTAzimuthalRoughness = attrTTAzimuthalRoughness;
    keys.mHairTTTintColor = attrTTTintColor;
    keys.mHairUseOptimizedSampling = attrUseOptimizedSampling;
    keys.mHairShowTRRTPlus = attrShowTRRTPlus;

    return keys;
}

} // end anonymous namespace

//---------------------------------------------------------------------------
/// @brief An Energy Conserving Hair Material based on the following papers:
/// [1] An Energy-conserving Hair Reflectance Model - D'eon et al Sig'11
/// [2] A Practical and Controllable Hair and Fur Model for Production Path Tracing - Chiang et al EGSR'16
/// [3] Importance Sampling for Physically-Based Hair Fiber Models, D'eon et al Sig'13
/// [4] Light Scattering from Human Hair Fibers - Marschner et al
RDL2_DSO_CLASS_BEGIN(HairMaterial_v3, DwaBase)

public:
    HairMaterial_v3(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name);
    ~HairMaterial_v3() { }

private:
    virtual void update();

    static void shade(const scene_rdl2::rdl2::Material* self,
                      moonray::shading::TLState *tls,
                      const moonray::shading::State& state,
                      moonray::shading::BsdfBuilder& bsdfBuilder);

    static float ior(const scene_rdl2::rdl2::Material* self,
                     moonray::shading::TLState *tls,
                     const moonray::shading::State& state);

RDL2_DSO_CLASS_END(HairMaterial_v3)

//---------------------------------------------------------------------------
HairMaterial_v3::HairMaterial_v3(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name) :
    DwaBase(sceneClass, name,
            collectAttributeKeys(),
            ispc::HairMaterial_v3_collectAttributeFuncs(),
            sLabels,
            ispc::Model::Hair)
{
    mType |= scene_rdl2::rdl2::INTERFACE_DWABASEHAIRLAYERABLE;

    mShadeFunc = HairMaterial_v3::shade;
    mShadeFuncv = (scene_rdl2::rdl2::ShadeFuncv) ispc::HairMaterial_v3_getShadeFunc();
    mIorFunc = HairMaterial_v3::ior;
}

void
HairMaterial_v3::update()
{
    // must call DwaBase::update()!
    DwaBase::update();
}

void
HairMaterial_v3::shade(const scene_rdl2::rdl2::Material* self,
                       moonray::shading::TLState *tls,
                       const moonray::shading::State &state,
                       moonray::shading::BsdfBuilder &bsdfBuilder)
{
    const HairMaterial_v3* me = static_cast<const HairMaterial_v3*>(self);
    const ispc::DwaBase* dwabase = me->getISPCBaseMaterialStruct();

    ispc::DwaBaseParameters params;
    me->resolveParameters(tls, state, me->get(attrCastsCaustics), params);
    me->createLobes(me, tls, state, bsdfBuilder, params, dwabase->mUParams, sLabels);
}

float
HairMaterial_v3::ior(const scene_rdl2::rdl2::Material* self,
                     moonray::shading::TLState* tls,
                     const moonray::shading::State& state)
{
    const HairMaterial_v3* me = static_cast<const HairMaterial_v3*>(self);
    return me->get(attrRefractiveIndex);
}


//---------------------------------------------------------------------------

