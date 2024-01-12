// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file DwaFabricMaterial.ispc

#include "attributes.isph"
#include "labels.isph"

#include <moonshine/material/dwabase/ispc/DwaBase.isph>
#include <moonshine/material/dwabase/ispc/DwaBaseLayerable.isph>
#include <moonray/rendering/shading/ispc/MaterialApi.isph>

export uniform DwaBaseAttributeFuncs DwaFabricMaterial_collectAttributeFuncs()
{
    uniform DwaBaseAttributeFuncs fns;
    DWABASE_initDwaBaseAttributeFuncs(fns); // set all struct members to 0 (null)

    ASSIGN_GLITTER_ATTR_FUNCS(fns);

    fns.mGetAttrShowFuzz                    = (uniform intptr_t) (void*) getAttrShowFuzz;
    fns.mEvalAttrFuzz                       = (uniform intptr_t) (void*) evalAttrFuzz;
    fns.mEvalAttrFuzzRoughness              = (uniform intptr_t) (void*) evalAttrFuzzRoughness;
    fns.mEvalAttrFuzzAlbedo                 = (uniform intptr_t) (void*) evalAttrFuzzAlbedo;
    fns.mGetAttrFuzzUseAbsorbingFibers      = (uniform intptr_t) (void*) getAttrFuzzUseAbsorbingFibers;
    fns.mEvalAttrFuzzNormalDial             = (uniform intptr_t) (void*) evalAttrFuzzNormalDial;

    /* fns.mGetAttrShowSpecular                = (uniform intptr_t) (void*) getAttrShowSpecular; */
    /* fns.mEvalAttrSpecular                   = (uniform intptr_t) (void*) evalAttrSpecular; */
    /* fns.mGetAttrRefractiveIndex             = (uniform intptr_t) (void*) getAttrRefractiveIndex; */
    /* fns.mEvalAttrMetallic                   = (uniform intptr_t) (void*) evalAttrMetallic; */
    /* fns.mEvalAttrMetallicColor              = (uniform intptr_t) (void*) evalAttrMetallicColor; */
    /* fns.mEvalAttrMetallicEdgeColor          = (uniform intptr_t) (void*) evalAttrMetallicEdgeColor; */
    /* fns.mEvalAttrRoughness                  = (uniform intptr_t) (void*) evalAttrRoughness; */
    /* fns.mEvalAttrAnisotropy                 = (uniform intptr_t) (void*) evalAttrAnisotropy; */
    /* fns.mEvalAttrShadingTangent             = (uniform intptr_t) (void*) evalAttrShadingTangent; */
    /* fns.mEvalAttrIridescence                = (uniform intptr_t) (void*) evalAttrIridescence; */
    /* fns.mEvalAttrIridescencePrimaryColor    = (uniform intptr_t) (void*) getAttrIridescencePrimaryColor; */
    /* fns.mEvalAttrIridescenceSecondaryColor  = (uniform intptr_t) (void*) getAttrIridescenceSecondaryColor; */
    /* fns.mGetAttrIridescenceFlipHueDirection = (uniform intptr_t) (void*) getAttrIridescenceFlipHueDirection; */
    /* fns.mEvalAttrIridescenceThickness       = (uniform intptr_t) (void*) evalAttrIridescenceThickness; */
    /* fns.mEvalAttrIridescenceExponent        = (uniform intptr_t) (void*) evalAttrIridescenceExponent; */
    /* fns.mEvalAttrIridescenceAt0             = (uniform intptr_t) (void*) evalAttrIridescenceAt0; */
    /* fns.mEvalAttrIridescenceAt90            = (uniform intptr_t) (void*) evalAttrIridescenceAt90; */
    /* fns.mGetAttrShowTransmission            = (uniform intptr_t) (void*) getAttrShowTransmission; */
    /* fns.mEvalAttrTransmission               = (uniform intptr_t) (void*) evalAttrTransmission; */
    /* fns.mEvalAttrTransmissionColor          = (uniform intptr_t) (void*) evalAttrTransmissionColor; */
    fns.mGetAttrShowDiffuse                 = (uniform intptr_t) (void*) getAttrShowDiffuse;
    fns.mEvalAttrAlbedo                     = (uniform intptr_t) (void*) evalAttrAlbedo;
    fns.mEvalAttrDiffuseRoughness           = (uniform intptr_t) (void*) evalAttrDiffuseRoughness;
    /* fns.mGetAttrBssrdf                      = (uniform intptr_t) (void*) getAttrBssrdf; */
    /* fns.mEvalAttrScatteringColor            = (uniform intptr_t) (void*) evalAttrScatteringColor; */
    /* fns.mEvalAttrScatteringRadius           = (uniform intptr_t) (void*) evalAttrScatteringRadius; */
    fns.mEvalAttrDiffuseTransmission        = (uniform intptr_t) (void*) evalAttrDiffuseTransmission;
    fns.mEvalAttrDiffuseTransmissionColor   = (uniform intptr_t) (void*) evalAttrDiffuseTransmissionColor;
    fns.mGetAttrDiffuseTransmissionBlendingBehavior = (uniform intptr_t) (void*) getAttrDiffuseTransmissionBlendingBehavior;
    fns.mGetAttrShowFabricSpecular          = (uniform intptr_t) (void*) getAttrShowFabricSpecular;
    /* fns.mEvalAttrFabricSpecular             = (uniform intptr_t) (void*) evalAttrFabricSpecular; */
    fns.mEvalAttrWarpColor                  = (uniform intptr_t) (void*) evalAttrWarpColor;
    fns.mEvalAttrWarpRoughness              = (uniform intptr_t) (void*) evalAttrWarpRoughness;
    fns.mGetAttrUseIndependentWeftAttributes    = (uniform intptr_t) (void*) getAttrUseIndependentWeftAttributes;
    fns.mEvalAttrWeftRoughness              = (uniform intptr_t) (void*) evalAttrWeftRoughness;
    fns.mEvalAttrWeftColor                  = (uniform intptr_t) (void*) evalAttrWeftColor;
    fns.mGetAttrUseUVsForThreadDirection    = (uniform intptr_t) (void*) getAttrUseUVsForThreadDirection;
    fns.mEvalAttrWarpThreadDirection        = (uniform intptr_t) (void*) evalAttrWarpThreadDirection;
    fns.mGetAttrWarpThreadCoverage          = (uniform intptr_t) (void*) getAttrWarpThreadCoverage;
    fns.mEvalAttrWarpThreadElevation        = (uniform intptr_t) (void*) evalAttrWarpThreadElevation;
    fns.mEvalAttrFabricDiffuseScattering    = (uniform intptr_t) (void*) evalAttrFabricDiffuseScattering;
    /* fns.mGetAttrShowClearcoat               = (uniform intptr_t) (void*) getAttrShowClearcoat; */
    /* fns.mEvalAttrClearcoat                  = (uniform intptr_t) (void*) evalAttrClearcoat; */
    /* fns.mGetAttrClearcoatRefractiveIndex    = (uniform intptr_t) (void*) getAttrClearcoatRefractiveIndex; */
    /* fns.mEvalAttrClearcoatRoughness         = (uniform intptr_t) (void*) evalAttrClearcoatRoughness; */
    /* fns.mEvalAttrClearcoatThickness         = (uniform intptr_t) (void*) evalAttrClearcoatThickness; */
    /* fns.mEvalAttrClearcoatAttenuationColor  = (uniform intptr_t) (void*) evalAttrClearcoatAttenuationColor; */
    /* fns.mGetAttrUsePhysicalClearcoatAbsorption  = (uniform intptr_t) (void*) getAttrUsePhysicalClearcoatAbsorption; */
    /* fns.mGetAttrUseClearcoatNormal          = (uniform intptr_t) (void*) getAttrUseClearcoatNormal; */
    /* fns.mGetAttrClearcoatNormalSpace        = (uniform intptr_t) (void*) getAttrClearcoatNormalSpace; */
    fns.mGetAttrShowEmission                = (uniform intptr_t) (void*) getAttrShowEmission;
    fns.mEvalAttrEmission                   = (uniform intptr_t) (void*) evalAttrEmission;
    fns.mEvalAttrPresence                   = (uniform intptr_t) (void*) evalAttrPresence;
    fns.mGetAttrThinGeometry                = (uniform intptr_t) (void*) getAttrThinGeometry;
    fns.mGetAttrCastsCaustics               = (uniform intptr_t) (void*) getAttrCastsCaustics;
    fns.mEvalAttrInputNormalDial            = (uniform intptr_t) (void*) evalAttrInputNormalDial;

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
    DWABASE_resolveParameters(me, tls, state, getAttrCastsCaustics(me), &params);
    DWABASE_createLobes(me, tls, state, bsdfBuilder, &params, &dwaBase->mUParams);
}
DEFINE_MATERIAL_SHADER(DwaFabricMaterial, shade)

