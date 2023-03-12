// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file DwaBaseMaterial.ispc

#include "attributes.isph"
#include "labels.isph"

#include <moonshine/material/dwabase/ispc/DwaBase.isph>
#include <moonshine/material/dwabase/ispc/DwaBaseLayerable.isph>
#include <moonray/rendering/shading/ispc/MaterialApi.isph>

export uniform DwaBaseAttributeFuncs DwaBaseMaterial_collectAttributeFuncs()
{
    uniform DwaBaseAttributeFuncs fns;
    DWABASE_initDwaBaseAttributeFuncs(fns); // set all struct members to 0 (null)

    ASSIGN_GLITTER_ATTR_FUNCS(fns);
    ASSIGN_IRIDESCENCE_ATTR_FUNCS(fns);

    fns.mGetAttrShowFuzz                    = (uniform intptr_t) (void*) getAttrShowFuzz;
    fns.mEvalAttrFuzz                       = (uniform intptr_t) (void*) evalAttrFuzz;
    fns.mEvalAttrFuzzRoughness              = (uniform intptr_t) (void*) evalAttrFuzzRoughness;
    fns.mEvalAttrFuzzAlbedo                 = (uniform intptr_t) (void*) evalAttrFuzzAlbedo;
    fns.mGetAttrFuzzUseAbsorbingFibers      = (uniform intptr_t) (void*) getAttrFuzzUseAbsorbingFibers;
    fns.mEvalAttrFuzzNormalDial             = (uniform intptr_t) (void*) evalAttrFuzzNormalDial;

    fns.mGetAttrShowSpecular                = (uniform intptr_t) (void*) getAttrShowSpecular;
    fns.mEvalAttrSpecular                   = (uniform intptr_t) (void*) evalAttrSpecular;
    fns.mGetAttrSpecularModel               = (uniform intptr_t) (void*) getAttrSpecularModel;
    fns.mGetAttrRefractiveIndex             = (uniform intptr_t) (void*) getAttrRefractiveIndex;
    fns.mEvalAttrMetallic                   = (uniform intptr_t) (void*) evalAttrMetallic;
    fns.mEvalAttrMetallicColor              = (uniform intptr_t) (void*) evalAttrMetallicColor;
    fns.mEvalAttrMetallicEdgeColor          = (uniform intptr_t) (void*) evalAttrMetallicEdgeColor;
    fns.mEvalAttrRoughness                  = (uniform intptr_t) (void*) evalAttrRoughness;
    fns.mEvalAttrAnisotropy                 = (uniform intptr_t) (void*) evalAttrAnisotropy;
    fns.mEvalAttrShadingTangent             = (uniform intptr_t) (void*) evalAttrShadingTangent;
    fns.mGetAttrShowTransmission            = (uniform intptr_t) (void*) getAttrShowTransmission;
    fns.mEvalAttrTransmission               = (uniform intptr_t) (void*) evalAttrTransmission;
    fns.mEvalAttrTransmissionColor          = (uniform intptr_t) (void*) evalAttrTransmissionColor;
    fns.mGetAttrUseIndependentTransmissionRefractiveIndex = (uniform intptr_t) (void*) getAttrUseIndependentTransmissionRefractiveIndex;
    fns.mGetAttrIndependentTransmissionRefractiveIndex    = (uniform intptr_t) (void*) getAttrIndependentTransmissionRefractiveIndex;
    fns.mGetAttrUseIndependentTransmissionRoughness = (uniform intptr_t) (void*) getAttrUseIndependentTransmissionRoughness;
    fns.mEvalAttrIndependentTransmissionRoughness   = (uniform intptr_t) (void*) evalAttrIndependentTransmissionRoughness;
    fns.mGetAttrUseDispersion               = (uniform intptr_t) (void*) getAttrUseDispersion;
    fns.mGetAttrDispersionAbbeNumber        = (uniform intptr_t) (void*) getAttrDispersionAbbeNumber;
    fns.mGetAttrShowDiffuse                 = (uniform intptr_t) (void*) getAttrShowDiffuse;
    fns.mEvalAttrAlbedo                     = (uniform intptr_t) (void*) evalAttrAlbedo;
    fns.mEvalAttrDiffuseRoughness           = (uniform intptr_t) (void*) evalAttrDiffuseRoughness;
    fns.mGetAttrSubsurface                  = (uniform intptr_t) (void*) getAttrSubsurface;
    fns.mEvalAttrScatteringColor            = (uniform intptr_t) (void*) evalAttrScatteringColor;
    fns.mEvalAttrScatteringRadius           = (uniform intptr_t) (void*) evalAttrScatteringRadius;
    fns.mGetAttrSSSResolveSelfIntersections = (uniform intptr_t) (void*) getAttrSSSResolveSelfIntersections;
    fns.mGetEnableSubsurfaceInputNormal     = (uniform intptr_t) (void*) getAttrEnableSubsurfaceInputNormal;
    fns.mEvalAttrDiffuseTransmission        = (uniform intptr_t) (void*) evalAttrDiffuseTransmission;
    fns.mEvalAttrDiffuseTransmissionColor   = (uniform intptr_t) (void*) evalAttrDiffuseTransmissionColor;
    fns.mGetAttrDiffuseTransmissionBlendingBehavior = (uniform intptr_t) (void*) getAttrDiffuseTransmissionBlendingBehavior;
    fns.mGetAttrShowOuterSpecular               = (uniform intptr_t) (void*) getAttrShowClearcoat;
    fns.mEvalAttrOuterSpecular                  = (uniform intptr_t) (void*) evalAttrClearcoat;
    fns.mGetAttrOuterSpecularModel              = (uniform intptr_t) (void*) getAttrClearcoatModel;
    fns.mGetAttrOuterSpecularRefractiveIndex    = (uniform intptr_t) (void*) getAttrClearcoatRefractiveIndex;
    fns.mEvalAttrOuterSpecularRoughness         = (uniform intptr_t) (void*) evalAttrClearcoatRoughness;
    fns.mEvalAttrOuterSpecularThickness         = (uniform intptr_t) (void*) evalAttrClearcoatThickness;
    fns.mEvalAttrOuterSpecularAttenuationColor  = (uniform intptr_t) (void*) evalAttrClearcoatAttenuationColor;
    fns.mGetAttrOuterSpecularUseBending         = (uniform intptr_t) (void*) getAttrClearcoatUseBending;
    fns.mGetAttrUseOuterSpecularNormal          = (uniform intptr_t) (void*) getAttrUseClearcoatNormal;
    fns.mEvalAttrOuterSpecularNormalDial        = (uniform intptr_t) (void*) evalAttrClearcoatNormalDial;
    fns.mEvalAttrInputNormalDial            = (uniform intptr_t) (void*) evalAttrInputNormalDial;
    fns.mGetAttrNormalAAStrategy            = (uniform intptr_t) (void*) getAttrNormalAAStrategy;
    fns.mGetAttrNormalAADial                = (uniform intptr_t) (void*) getAttrNormalAADial;
    fns.mGetAttrShowEmission                = (uniform intptr_t) (void*) getAttrShowEmission;
    fns.mEvalAttrEmission                   = (uniform intptr_t) (void*) evalAttrEmission;
    fns.mEvalAttrPresence                   = (uniform intptr_t) (void*) evalAttrPresence;
    fns.mGetAttrThinGeometry                = (uniform intptr_t) (void*) getAttrThinGeometry;
    fns.mGetAttrCastsCaustics               = (uniform intptr_t) (void*) getAttrCastsCaustics;

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
DEFINE_MATERIAL_SHADER(DwaBaseMaterial, shade)

