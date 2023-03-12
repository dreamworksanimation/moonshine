// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file HairMaterialv3.ispc

#include "attributes.isph"
#include "labels.isph"

#include <moonshine/material/dwabase/ispc/DwaBase.isph>
#include <moonshine/material/dwabase/ispc/DwaBaseLayerable.isph>
#include <moonray/rendering/shading/ispc/MaterialApi.isph>

export uniform DwaBaseAttributeFuncs HairMaterial_v3_collectAttributeFuncs()
{
    uniform DwaBaseAttributeFuncs fns;
    DWABASE_initDwaBaseAttributeFuncs(fns); // set all struct members to 0 (null)

    fns.mGetAttrCastsCaustics              = (uniform intptr_t) (void*) getAttrCastsCaustics;
    fns.mEvalAttrPresence                  = (uniform intptr_t) (void*) evalAttrPresence;
    fns.mGetAttrShowEmission               = (uniform intptr_t) (void*) getAttrShowEmission;
    fns.mEvalAttrEmission                  = (uniform intptr_t) (void*) evalAttrEmission;

    fns.mEvalAttrHairColor                 = (uniform intptr_t) (void*) evalAttrHairColor;
    fns.mGetAttrRefractiveIndex            = (uniform intptr_t) (void*) getAttrRefractiveIndex;
    fns.mGetAttrHairFresnelType            = (uniform intptr_t) (void*) getAttrFresnelType;
    fns.mEvalAttrHairCuticleLayerThickness = (uniform intptr_t) (void*) evalAttrCuticleLayerThickness;
    fns.mGetAttrHairShowR                  = (uniform intptr_t) (void*) getAttrShowR;
    fns.mEvalAttrHairROffset               = (uniform intptr_t) (void*) evalAttrROffset;
    fns.mEvalAttrHairRRoughness            = (uniform intptr_t) (void*) evalAttrRRoughness;
    fns.mEvalAttrHairRTintColor            = (uniform intptr_t) (void*) evalAttrRTintColor;
    fns.mGetAttrHairShowTRT                = (uniform intptr_t) (void*) getAttrShowTRT;
    fns.mEvalAttrHairTRTOffset             = (uniform intptr_t) (void*) evalAttrTRTOffset;
    fns.mGetAttrHairTRTUseRoughness        = (uniform intptr_t) (void*) getAttrUseTRTRoughness;
    fns.mEvalAttrHairTRTRoughness          = (uniform intptr_t) (void*) evalAttrTRTRoughness;
    fns.mEvalAttrHairTRTTintColor          = (uniform intptr_t) (void*) evalAttrTRTTintColor;
    fns.mGetAttrHairShowTT                 = (uniform intptr_t) (void*) getAttrShowTT;
    fns.mEvalAttrHairTTOffset              = (uniform intptr_t) (void*) evalAttrTTOffset;
    fns.mGetAttrHairTTUseRoughness         = (uniform intptr_t) (void*) getAttrUseTTRoughness;
    fns.mEvalAttrHairTTRoughness           = (uniform intptr_t) (void*) evalAttrTTRoughness;
    fns.mEvalAttrHairTTSaturation          = (uniform intptr_t) (void*) evalAttrTTSaturation;
    fns.mEvalAttrHairTTAzimuthalRoughness  = (uniform intptr_t) (void*) evalAttrTTAzimuthalRoughness;
    fns.mEvalAttrHairTTTintColor           = (uniform intptr_t) (void*) evalAttrTTTintColor;
    fns.mGetAttrShowHairGlint              = (uniform intptr_t) (void*) getAttrShowHairGlint;
    fns.mEvalAttrHairGlintRoughness        = (uniform intptr_t) (void*) evalAttrGlintRoughness;
    fns.mEvalAttrHairGlintMinTwists        = (uniform intptr_t) (void*) evalAttrGlintMinTwists;
    fns.mEvalAttrHairGlintMaxTwists        = (uniform intptr_t) (void*) evalAttrGlintMaxTwists;
    fns.mEvalAttrHairGlintEccentricity     = (uniform intptr_t) (void*) evalAttrGlintEccentricity;
    fns.mEvalAttrHairGlintSaturation       = (uniform intptr_t) (void*) evalAttrGlintSaturation;
    fns.mGetAttrHairUseOptimizedSampling   = (uniform intptr_t) (void*) getAttrUseOptimizedSampling;
    fns.mGetAttrHairShowTRRTPlus           = (uniform intptr_t) (void*) getAttrShowTRRTPlus;

    return fns;
}

static void
shade(const uniform Material *      uniform  me,
            uniform ShadingTLState *uniform  tls,
      const varying State                    &state,
            varying BsdfBuilder              &bsdfBuilder)
{
    const uniform DwaBase * uniform dwaBase =
                    (const uniform DwaBase* uniform)getDwaBaseMaterialStruct(me);
    varying DwaBaseParameters params;
    DWABASE_resolveParameters(me, tls, state, getAttrCastsCaustics(me), &params);
    DWABASE_createLobes(me, tls, state, bsdfBuilder, &params, &dwaBase->mUParams);
}

DEFINE_MATERIAL_SHADER(HairMaterial_v3, shade)

