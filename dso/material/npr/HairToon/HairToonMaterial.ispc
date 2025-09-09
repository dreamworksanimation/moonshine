// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file HairToonMaterial.ispc

#include "attributes.isph"
#include "labels.isph"

#include <moonray/rendering/shading/ispc/MaterialApi.isph>

#include <moonshine/material/dwabase/ispc/DwaBase.isph>
#include <moonshine/material/dwabase/ispc/DwaBaseLayerable.isph>

export uniform DwaBaseAttributeFuncs HairToonMaterial_collectAttributeFuncs()
{
    uniform DwaBaseAttributeFuncs fns;
    DWABASE_initDwaBaseAttributeFuncs(fns); // set all struct members to 0 (null)

    fns.mEvalAttrPresence                  = (uniform intptr_t) (void*) evalAttrPresence;
    fns.mGetAttrShowEmission               = (uniform intptr_t) (void*) getAttrShowEmission;
    fns.mEvalAttrEmission                  = (uniform intptr_t) (void*) evalAttrEmission;

    fns.mGetAttrShowHairDiffuse            = (uniform intptr_t) (void*) getAttrShowHairDiffuse;
    fns.mEvalAttrHairDiffuse               = (uniform intptr_t) (void*) evalAttrHairDiffuse;

    fns.mEvalAttrHairColor                 = (uniform intptr_t) (void*) evalAttrHairColor;
    fns.mGetAttrHairDiffuseUseIndependentFrontAndBackColor
                                           = (uniform intptr_t) (void*) getAttrUseIndependentFrontAndBackHairColor;
    fns.mEvalAttrHairDiffuseFrontColor     = (uniform intptr_t) (void*) evalAttrFrontHairColor;
    fns.mEvalAttrHairDiffuseBackColor      = (uniform intptr_t) (void*) evalAttrBackHairColor;

    fns.mGetAttrSubsurface                  = (uniform intptr_t) (void*) getAttrHairSubsurface;
    fns.mEvalAttrScatteringColor            = (uniform intptr_t) (void*) evalAttrHairScatteringColor;
    fns.mEvalAttrScatteringRadius           = (uniform intptr_t) (void*) evalAttrHairScatteringRadius;
    fns.mGetEnableSubsurfaceInputNormal     = (uniform intptr_t) (void*) getAttrHairEnableSubsurfaceInputNormal;
    fns.mEvalAttrHairSubsurfaceBlend        = (uniform intptr_t) (void*) evalAttrHairSubsurfaceBlend;

    fns.mEvalAttrInputNormalDial            = (uniform intptr_t) (void*) evalAttrHairInputNormalDial;

    fns.mHairToonS1Funcs.mGetAttrShow            = (uniform intptr_t) (void*) getAttrHairToonS1Show;
    fns.mHairToonS1Funcs.mEvalAttrRoughness      = (uniform intptr_t) (void*) evalAttrHairToonS1Roughness;                     \
    ASSIGN_TOON_SPEC_ATTR_FUNCS(fns, HairToonS1);

    fns.mHairToonS2Funcs.mGetAttrShow            = (uniform intptr_t) (void*) getAttrHairToonS2Show;
    fns.mHairToonS2Funcs.mEvalAttrRoughness      = (uniform intptr_t) (void*) evalAttrHairToonS2Roughness;                     \
    ASSIGN_TOON_SPEC_ATTR_FUNCS(fns, HairToonS2);

    fns.mHairToonS3Funcs.mGetAttrShow            = (uniform intptr_t) (void*) getAttrHairToonS3Show;
    fns.mHairToonS3Funcs.mEvalAttrRoughness      = (uniform intptr_t) (void*) evalAttrHairToonS3Roughness;                     \
    ASSIGN_TOON_SPEC_ATTR_FUNCS(fns, HairToonS3);

    return fns;
}

static void
shade(const uniform Material *      uniform  me,
            uniform ShadingTLState *uniform  tls,
      const varying State                   &state,
            varying BsdfBuilder             &bsdfBuilder)
{
    const uniform DwaBase * uniform dwaBase =
                (const uniform DwaBase* uniform)getDwaBaseMaterialStruct(me);
    varying DwaBaseParameters params;
    DWABASELAYERABLE_initColorCorrectParameters(&params);
    DWABASE_resolveParameters(me, tls, state, false, &params);
    DWABASE_createLobes(me, tls, state, bsdfBuilder, &params, &dwaBase->mUParams);
}
DEFINE_MATERIAL_SHADER(HairToonMaterial, shade)

