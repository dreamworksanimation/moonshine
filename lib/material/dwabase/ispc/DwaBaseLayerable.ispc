// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file DwaBaseLayerable.ispc

#include "DwaBaseLayerable.isph"
#include <moonray/rendering/shading/ispc/Shading.isph>

ISPC_UTIL_EXPORT_ENUM_TO_HEADER(DWABASE_ToonDiffuseConstants);
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(DWABASE_IridescenceConstants);
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(ToonDiffuseModel);
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(ToonSpecularModel);
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(IridescenceLobe);
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(DwaBaseLayerable);
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(DwaBaseLabels);
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(DwaBaseParameters);
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(DwaBaseUniformParameters);
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(ToonDiffuseParameters);
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(ToonSpecularParameters);
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(DiffuseTransmissionBlendingBehavior);
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(DwaBaseEventMessages);

// Should remain in sync with BsdfFabricVelvet.isph
enum VelvetVersion {
    VELVET_VERSION_1,
    VELVET_VERSION_2
};

// Access Function to retrieve ISPC LABELS struct
extern "C" const void* uniform getLabelsDataPtr(const uniform Material* uniform);

extern "C" const void* uniform getISPCDwaBaseLayerablePtr(const uniform Material* uniform);

// When no label present set to invalid value defined in Bsdf.isph::BsdfLobe_setLabel.
static const uniform int noLabel = -1;

inline varying float
computeMicrofacetRoughness(varying float roughness,
                           varying int normalAAStrategy,
                           varying float normalLength,
                           varying float normalAADial,
                           varying float normalDial)
{
    if (normalAAStrategy != NORMAL_AA_STRATEGY_TOKSVIG) {
        // no special normal mapping AA strategy, exit early
        return roughness;
    }

    // Assuming NORMAL_AA_STRATEGY_TOKSVIG...

    // Exit early if normalLength is exactly 1.0. This suggests no input normal map is bound
    // or use independent clearcoat normal is false so the state N is being used,
    // or the evaluated normal map is exactly unit length. Also check if the dials are zero,
    // in any of these cases we can exit early.
    if (isOne(normalLength) || isZero(normalAADial) || isZero(normalDial)) {
        return roughness;
    }

    // Account for normalDial by lerping the normal map normal length with the
    // unit length (1.f) state normal.
    normalLength = lerp(1.f, normalLength, normalDial);
    const varying float varN = clamp(normalLength, 0.01f, 1.0f);
    const varying float toksvigFactor = max((1.0f - varN) / varN, 0.f);

    // Convolution of two gaussian distributions -> variance = sqrt(v1^2 + v2^2).
    const varying float roughnessAA = sqrt(toksvigFactor + roughness * roughness);

    return lerp(roughness, roughnessAA, normalAADial);
}

inline varying Vec2f
computeAnisoRoughness(const varying float roughness,
                      const varying float minRoughness,
                      const varying float anisotropy)
{
    varying Vec2f result;

    result.x = anisotropy > 0.0f ?
               roughness * (1.0f - anisotropy) : roughness;
    result.y = anisotropy < 0.0f ?
               roughness * (1.0f - abs(anisotropy)) : roughness;

    result.x = max(result.x, minRoughness);
    result.y = max(result.y, minRoughness);

    return result;
}

inline varying bool
isWhite(const varying Color& c)
{
    return (isOne(c.r) && isOne(c.g) && isOne(c.b));
}

varying float
createHairRandomPerStrand(const uniform Material * uniform me,
                          uniform ShadingTLState * uniform tls,
                          const varying State &state,
                          const varying HairParameters * uniform hairParams,
                          const DwaBaseEventMessages * uniform eventMessages)
{
    varying float glintRandom = 0.0f;
    if (!isEqual(hairParams->mHairGlintMinTwists, hairParams->mHairGlintMaxTwists)) {
        const uniform DwaBaseLayerable * uniform dwabaselayerable =
            (const uniform DwaBaseLayerable* uniform) getISPCDwaBaseLayerablePtr(me);

        // scatter_tag is a random number [0, 1) assigned to each hair
        float scatterTag = getFloatAttribute(tls, state, dwabaselayerable->mScatterTagKey);
        if (isEqual(scatterTag, -1.f)) {
            logEvent(me, tls, eventMessages->sErrorScatterTagMissing);
        } else {
            glintRandom = scatterTag;
        }
    }
    return glintRandom;
}

void
DWABASE_addGlitterLobes(const uniform Material * uniform me,
                        varying BsdfBuilder &builder,
                        uniform ShadingTLState * uniform tls,
                        const varying State &state,
                        const varying DwaBaseParameters * uniform params,
                        const uniform DwaBaseLabels * uniform labels,
                        uniform bool& exitEarly,
                        const DwaBaseEventMessages * uniform eventMessages)
{
    exitEarly = false;

    if (params->mGlitterPointerVector) {
        GLITTER_ResultCode resultCode = GLITTER_RESULTCODE_SUCCESS;
        GLITTER_createLobes(params->mGlitterPointerVector,
                            builder,
                            tls,
                            state,
                            params->mGlitterUniformParameters,
                            params->mGlitterVaryingParameters,
                            labels->mGlitter,
                            resultCode);

        if (resultCode != GLITTER_RESULTCODE_SUCCESS) {
            switch (resultCode)
            {
            case GLITTER_RESULTCODE_NO_REFN:
                logEvent(me, tls, eventMessages->sErrorNoRefN);
                break;
            case GLITTER_RESULTCODE_NO_REFP_PARTIALS:
                logEvent(me, tls, eventMessages->sWarnNoRefPpartials);
                break;
            default:
                break;
            }
        }
    }
}

Color
DWABASE_getDiffuseTransmissionAttenuation(const varying DwaBaseParameters * uniform params)
{
    varying Color diffuseTransmissionAttenuation;
    if (params->mDiffuseTransmissionBlendingBehavior == DIFFUSE_TRANSMISSION_BLENDING_RGB) {
        diffuseTransmissionAttenuation = sWhite - params->mDiffuseTransmission;
    } else {
        // Use the max of the transmission color to avoid hue shifting
        diffuseTransmissionAttenuation = sWhite - Color_ctor(
                max(max(params->mDiffuseTransmission.r,
                        params->mDiffuseTransmission.g),
                        params->mDiffuseTransmission.b));
    }
    return diffuseTransmissionAttenuation;
}

void
DWABASE_addHairLobes(const uniform Material * uniform me,
                     uniform ShadingTLState * uniform tls,
                     const varying State &state,
                     varying BsdfBuilder &bsdfBuilder,
                     const varying DwaBaseParameters * uniform params,
                     const uniform DwaBaseUniformParameters * uniform uParams,
                     const uniform DwaBaseLabels * uniform labels,
                     const DwaBaseEventMessages * uniform eventMessages)
{
    const varying HairParameters * uniform hairParams = &params->mHairParameters;

    // Hair Diffuse
    if (!isZero(hairParams->mHairDiffuse)) {

        // hair curves are always thin
        BsdfBuilder_setThinGeo(bsdfBuilder);

        const bool hasSSS = !isBlack(params->mScatteringRadius);
        const float sssWeight = hasSSS ? hairParams->mHairSubsurfaceBlend : 0.0f;

        HairDiffuseBSDF hairDiffuseBsdf;
        HairDiffuseBSDF_init(hairDiffuseBsdf,
                             hairParams->mHairDir,
                             hairParams->mHairDiffuseFrontColor,
                             hairParams->mHairDiffuseBackColor);

        BsdfBuilder_addHairDiffuseBSDF(bsdfBuilder,
                                       hairDiffuseBsdf,
                                       hairParams->mHairDiffuse * (1.0f - sssWeight),
                                       BSDFBUILDER_PHYSICAL,
                                       labels->mHairLabels.mHair);

        if (hasSSS) {
            // diffuse front color substitutes for albedo in these lobes
            // random walk is not supported
            Vec3f N = getN(state);

            const SubsurfaceType bssrdfType = (SubsurfaceType)uParams->mSubsurface;
            if (bssrdfType == SUBSURFACE_NORMALIZED_DIFFUSION) {
                NormalizedDiffusion diffuseRefl;
                NormalizedDiffusion_init(diffuseRefl,
                                         N,
                                         hairParams->mHairDiffuseFrontColor,
                                         params->mScatteringRadius,
                                         me,
                                         params->mSubsurfaceTraceSet,
                                         params->mEvalSubsurfaceNormalFn);

                BsdfBuilder_addNormalizedDiffusion(bsdfBuilder, 
                                                   diffuseRefl,
                                                   hairParams->mHairDiffuse * sssWeight,
                                                   BSDFBUILDER_PHYSICAL,
                                                   labels->mDiffuse);

            } else if (bssrdfType == SUBSURFACE_DIPOLE_DIFFUSION) {
                DipoleDiffusion diffuseRefl;
                DipoleDiffusion_init(diffuseRefl,
                                     N,
                                     hairParams->mHairDiffuseFrontColor,
                                     params->mScatteringRadius,
                                     me,
                                     params->mSubsurfaceTraceSet,
                                     params->mEvalSubsurfaceNormalFn);

                BsdfBuilder_addDipoleDiffusion(bsdfBuilder, 
                                               diffuseRefl,
                                               hairParams->mHairDiffuse * sssWeight,
                                               BSDFBUILDER_PHYSICAL,
                                               labels->mDiffuse);

            }
        }
    }

    // Hair
    if (!isZero(hairParams->mHair)) {

        // hair curves are always thin
        BsdfBuilder_setThinGeo(bsdfBuilder);

        if (!hairParams->mHairCastsCaustics) {
            HairDiffuseBSDF hairDiffuseBsdf;
            HairDiffuseBSDF_init(hairDiffuseBsdf,
                                 hairParams->mHairDir,
                                 hairParams->mHairColor,
                                 hairParams->mHairColor);

            BsdfBuilder_addHairDiffuseBSDF(bsdfBuilder,
                                           hairDiffuseBsdf,
                                           1.0f,
                                           BSDFBUILDER_PHYSICAL,
                                           labels->mHairLabels.mHair);
        } else {
            Vec3f N = Vec3f_ctor(0.0f);
            float hairRotation = 0.0f;
            float glintRoughness = 0.0f;
            if (hairParams->mHairShowGlint) {
                if (!(isZero(hairParams->mHairGlintMinTwists) &&
                      isZero(hairParams->mHairGlintMaxTwists))) {
                    const Vec3f T = normalize(getdPds(state));
                    const Vec3f B = normalize(getdPdt(state));
                    N = cross(B, T);

                    // Random float per hair between [0, 1)
                    const float glintRandom = createHairRandomPerStrand(me,
                                                                        tls,
                                                                        state,
                                                                        hairParams,
                                                                        eventMessages);

                    hairRotation = sTwoPi * lerp(hairParams->mHairGlintMinTwists,
                                                 hairParams->mHairGlintMaxTwists,
                                                 glintRandom);
                }

                // Somewhat arbitrary remapping based on empirical results
                glintRoughness = 0.125f * hairParams->mHairGlintRoughness * hairParams->mHairGlintRoughness;
            }

            if (hairParams->mHairUseOptimizedSampling) {

                HairBSDF hairBsdf;
                HairBSDF_init(hairBsdf,
                              hairParams->mHairDir,
                              hairParams->mHairUV,
                              hairParams->mHairIOR,
                              hairParams->mHairFresnelType,
                              hairParams->mHairCuticleLayerThickness,
                              hairParams->mHairShowR,
                              hairParams->mHairRShift,
                              hairParams->mHairRLongRoughness,
                              hairParams->mHairRTint,
                              hairParams->mHairShowTT,
                              hairParams->mHairTTShift,
                              hairParams->mHairTTLongRoughness,
                              hairParams->mHairTTAzimRoughness,
                              hairParams->mHairTTTint,
                              hairParams->mHairTTSaturation,
                              hairParams->mHairShowTRT,
                              hairParams->mHairTRTShift,
                              hairParams->mHairTRTLongRoughness,
                              hairParams->mHairTRTTint,
                              hairParams->mHairShowGlint,
                              glintRoughness,
                              clamp(hairParams->mHairGlintEccentricity, 0.85f, 1.0f),
                              hairParams->mHairGlintSaturation,
                              hairRotation,
                              N,
                              hairParams->mHairShowTRRT,
                              hairParams->mHairColor);

                BsdfBuilder_addHairBSDF(bsdfBuilder,
                                        hairBsdf,
                                        1.0f,
                                        BSDFBUILDER_PHYSICAL,
                                        labels->mHairLabels.mHair);
            } else {
                if (hairParams->mHairShowR) {
                    HairRBRDF hairRBRDF;
                    HairRBRDF_init(hairRBRDF,
                                   hairParams->mHairDir,
                                   hairParams->mHairUV,
                                   hairParams->mHairIOR,
                                   hairParams->mHairFresnelType,
                                   hairParams->mHairCuticleLayerThickness,
                                   hairParams->mHairRShift,
                                   hairParams->mHairRLongRoughness,
                                   (varying Color) sWhite);

                    BsdfBuilder_addHairRBRDF(bsdfBuilder,
                                             hairRBRDF,
                                             1.0f,
                                             BSDFBUILDER_PHYSICAL,
                                             labels->mHairLabels.mHairR);
                }

                // these 3 lobes need to be added as adjacent lobes because while
                // the are 'under' the R lobe, they do not effect each other's
                // energy distribution
                BsdfBuilder_startAdjacentComponents(bsdfBuilder);

                if (hairParams->mHairShowTT) {
                    HairTTBTDF hairTTBTDF;
                    HairTTBTDF_init(hairTTBTDF,
                                    hairParams->mHairDir,
                                    hairParams->mHairUV,
                                    hairParams->mHairIOR,
                                    hairParams->mHairFresnelType,
                                    hairParams->mHairCuticleLayerThickness,
                                    hairParams->mHairTTShift,
                                    hairParams->mHairTTLongRoughness,
                                    hairParams->mHairTTAzimRoughness,
                                    hairParams->mHairColor,
                                    hairParams->mHairTTTint,
                                    hairParams->mHairTTSaturation);

                    BsdfBuilder_addHairTTBTDF(bsdfBuilder,
                                              hairTTBTDF,
                                              1.0f,
                                              BSDFBUILDER_PHYSICAL,
                                              labels->mHairLabels.mHairTT);
                }

                if (hairParams->mHairShowTRT) {
                    HairTRTBRDF hairTRTBRDF;
                    HairTRTBRDF_init(hairTRTBRDF,
                                     hairParams->mHairDir,
                                     hairParams->mHairUV,
                                     hairParams->mHairIOR,
                                     hairParams->mHairFresnelType,
                                     hairParams->mHairCuticleLayerThickness,
                                     hairParams->mHairTRTShift,
                                     hairParams->mHairTRTLongRoughness,
                                     hairParams->mHairTTAzimRoughness,
                                     hairParams->mHairColor,
                                     hairParams->mHairTRTTint,
                                     hairParams->mHairShowGlint,
                                     glintRoughness,
                                     clamp(hairParams->mHairGlintEccentricity, 0.85f, 1.0f),
                                     hairParams->mHairGlintSaturation,
                                     hairRotation,
                                     N);

                    BsdfBuilder_addHairTRTBRDF(bsdfBuilder,
                                               hairTRTBRDF,
                                               1.0f,
                                               BSDFBUILDER_PHYSICAL,
                                               labels->mHairLabels.mHairTRT);
                }

                if (hairParams->mHairShowTRRT) {
                    HairTRRTBRDF hairTRRTBRDF;
                    HairTRRTBRDF_init(hairTRRTBRDF,
                                      hairParams->mHairDir,
                                      hairParams->mHairUV,
                                      hairParams->mHairIOR,
                                      hairParams->mHairFresnelType,
                                      hairParams->mHairCuticleLayerThickness,
                                      hairParams->mHairTRRTLongRoughness,
                                      hairParams->mHairTTAzimRoughness,
                                      hairParams->mHairColor,
                                      (varying Color) sWhite);

                    BsdfBuilder_addHairTRRTBRDF(bsdfBuilder,
                                                hairTRRTBRDF,
                                                1.0f,
                                                BSDFBUILDER_PHYSICAL,
                                                labels->mHairLabels.mHairTRRT);
                }

                BsdfBuilder_endAdjacentComponents(bsdfBuilder);
            }
        }
    }
}

void
DWABASE_addToonSpecularLobes(varying BsdfBuilder &bsdfBuilder,
                             const varying ToonSpecularParameters& params,
                             const uniform DwaBaseLabels * uniform labels)
{
    ToonSpecularBRDF toonSpecularBRDF;
    ToonSpecularBRDF_init(toonSpecularBRDF,
                          params.mNormal,
                          params.mIntensity,
                          params.mRoughness,
                          params.mTint,
                          params.mRampNumPoints,
                          params.mRampPositions,
                          params.mRampInterpolators,
                          params.mRampValues,
                          params.mStretchU,
                          params.mStretchV,
                          params.mdPds,
                          params.mdPdt,
                          params.mEnableIndirectReflections,
                          params.mIndirectReflectionsIntensity,
                          params.mIndirectReflectionsRoughness);

    BsdfBuilder_addToonSpecularBRDF(bsdfBuilder,
                                    toonSpecularBRDF,
                                    params.mToonSpecular, // weight
                                    BSDFBUILDER_PHYSICAL,
                                    labels->mSpecular);
}

void
DWABASE_addHairToonSpecularLobes(varying BsdfBuilder &bsdfBuilder,
                                 const varying ToonSpecularParameters& params,
                                 const uniform DwaBaseLabels * uniform labels)
{
    HairToonSpecularBRDF hairToonSpecularBRDF;
    HairToonSpecularBRDF_init(hairToonSpecularBRDF,
                              params.mNormal,
                              params.mHairDir,
                              params.mHairUV,
                              params.mHairIOR,
                              params.mHairFresnelType,
                              params.mHairCuticleLayerThickness,
                              params.mHairRShift,
                              params.mRoughness,
                              params.mTint,
                              params.mIntensity,
                              params.mRampNumPoints,
                              params.mRampPositions,
                              params.mRampInterpolators,
                              params.mRampValues,
                              params.mEnableIndirectReflections,
                              params.mIndirectReflectionsIntensity,
                              params.mIndirectReflectionsRoughness);

    BsdfBuilder_addHairToonSpecularBRDF(bsdfBuilder,
                                        hairToonSpecularBRDF,
                                        params.mToonSpecular, // weight
                                        BSDFBUILDER_PHYSICAL,
                                        labels->mSpecular);
}

void
DWABASE_addOuterSpecularLobes(varying BsdfBuilder &bsdfBuilder,
                              const varying DwaBaseParameters * uniform params,
                              const uniform DwaBaseUniformParameters * uniform uParams,
                              const uniform DwaBaseLabels * uniform labels,
                              varying float minRoughness,
                              varying Iridescence * uniform iridescence)
{
    const uniform bool refracts =
        (uParams->mOuterSpecularUseBending && (!uParams->mThinGeometry));

    const varying Color& attenuationColor = params->mOuterSpecularAttenuationColor;
    const varying bool absorbs = !isZero(params->mOuterSpecularThickness) &&
                                 !isWhite(attenuationColor);

    // Modulate outer specular roughness based on normal map mip-map anti-aliasing strategy.
    varying float roughness = computeMicrofacetRoughness(
            params->mOuterSpecularRoughness,
            params->mNormalAAStrategy,
            params->mOuterSpecularNormalLength,
            params->mNormalAADial,
            params->mOuterSpecularNormalDial);

    roughness = max(roughness, minRoughness);

    if (refracts || absorbs) {
        // only isotropic clearcoat supported in dwabase
        if (isZero(roughness)) { // mirror
            MirrorClearcoat clearcoat;
            MirrorClearcoat_init(
                    clearcoat,
                    params->mOuterSpecularNormal,
                    params->mOuterSpecularRefractiveIndex,
                    params->mOuterSpecularThickness,
                    attenuationColor,
                    refracts,
                    iridescence);

            BsdfBuilder_addMirrorClearcoat(
                    bsdfBuilder,
                    clearcoat,
                    params->mOuterSpecular,
                    BSDFBUILDER_PHYSICAL,
                    labels->mOuterSpecular);
        } else { // microfacet
            MicrofacetIsotropicClearcoat clearcoat;
            MicrofacetIsotropicClearcoat_init(
                    clearcoat,
                    params->mOuterSpecularNormal,
                    params->mOuterSpecularRefractiveIndex,
                    roughness,
                    params->mOuterSpecularThickness,
                    attenuationColor,
                    refracts,
                    (MicrofacetDistribution) uParams->mOuterSpecularModel,
                    MICROFACET_GEOMETRIC_SMITH,
                    iridescence);

            BsdfBuilder_addMicrofacetIsotropicClearcoat(
                    bsdfBuilder,
                    clearcoat,
                    params->mOuterSpecular,
                    BSDFBUILDER_PHYSICAL,
                    labels->mOuterSpecular);
        }
    } else { // not clearcoat, just a reflection lobe
        if (isZero(roughness)) { // mirror
            MirrorBRDF reflection;
            MirrorBRDF_init(
                    reflection,
                    params->mOuterSpecularNormal,
                    params->mOuterSpecularRefractiveIndex,
                    iridescence);

            BsdfBuilder_addMirrorBRDF(
                    bsdfBuilder,
                    reflection,
                    params->mOuterSpecular,
                    BSDFBUILDER_PHYSICAL,
                    labels->mOuterSpecular);
        } else { // microfacet
            MicrofacetIsotropicBRDF reflection;
            MicrofacetIsotropicBRDF_init(
                    reflection,
                    params->mOuterSpecularNormal,
                    params->mOuterSpecularRefractiveIndex,
                    roughness,
                    (MicrofacetDistribution) uParams->mOuterSpecularModel,
                    MICROFACET_GEOMETRIC_SMITH,
                    iridescence);

            BsdfBuilder_addMicrofacetIsotropicBRDF(
                    bsdfBuilder,
                    reflection,
                    params->mOuterSpecular,
                    BSDFBUILDER_PHYSICAL,
                    labels->mOuterSpecular);
        }
    }
}

void
DWABASE_addIndependentReflectionLobe(varying BsdfBuilder &bsdfBuilder,
                                     const varying Vec3f &N,
                                     varying float reflIor,
                                     varying float anisotropy,
                                     varying float roughness,
                                     varying float minRoughness,
                                     const varying Vec3f &shadingTangent,
                                     const uniform MicrofacetDistribution &specularModel,
                                     varying float reflectionWeight,
                                     varying Iridescence * uniform iridescence,
                                     uniform int label)
{
    const varying float roughnessClamped = max(roughness, minRoughness);

    if (roughnessClamped < sEpsilon) {
        MirrorBRDF dielectricBRDF;
        MirrorBRDF_init(dielectricBRDF,
                N,
                reflIor,
                iridescence);

        BsdfBuilder_addMirrorBRDF(
                bsdfBuilder,
                dielectricBRDF,
                reflectionWeight,
                BSDFBUILDER_PHYSICAL,
                label);
    } else {
        if (isZero(anisotropy)) {
            MicrofacetIsotropicBRDF dielectricBRDF;
            MicrofacetIsotropicBRDF_init(dielectricBRDF,
                    N,
                    reflIor,
                    roughnessClamped,
                    specularModel,
                    MICROFACET_GEOMETRIC_SMITH,
                    iridescence);

            BsdfBuilder_addMicrofacetIsotropicBRDF(
                    bsdfBuilder,
                    dielectricBRDF,
                    reflectionWeight,
                    BSDFBUILDER_PHYSICAL,
                    label);
        } else {
            const varying Vec2f rgh = computeAnisoRoughness(roughness,
                                                            minRoughness,
                                                            anisotropy);

            MicrofacetAnisotropicBRDF dielectricBRDF;
            MicrofacetAnisotropicBRDF_init(dielectricBRDF,
                    N,
                    reflIor,
                    rgh.x, rgh.y,
                    shadingTangent,
                    specularModel,
                    MICROFACET_GEOMETRIC_SMITH,
                    iridescence);

            BsdfBuilder_addMicrofacetAnisotropicBRDF(
                    bsdfBuilder,
                    dielectricBRDF,
                    reflectionWeight,
                    BSDFBUILDER_PHYSICAL,
                    label);
        }
    }
}

void
DWABASE_addIndependentTransmissionLobe(varying BsdfBuilder &bsdfBuilder,
                                       const varying Vec3f &N,
                                       varying float refrIor,
                                       varying float anisotropy,
                                       varying float transRoughness,
                                       varying float minRoughness,
                                       const varying Vec3f &shadingTangent,
                                       const uniform MicrofacetDistribution &specularModel,
                                       const varying Color &transmissionColor,
                                       varying float abbeNumber,
                                       varying float transmissionWeight,
                                       uniform int label)
{
    const varying float transRoughnessClamped = max(transRoughness, minRoughness);

    if (transRoughnessClamped < sEpsilon) {
        MirrorBTDF dielectricBTDF;
        MirrorBTDF_init(dielectricBTDF,
                N,
                refrIor,
                transmissionColor,
                abbeNumber);

        BsdfBuilder_addMirrorBTDF(
                bsdfBuilder,
                dielectricBTDF,
                transmissionWeight,
                BSDFBUILDER_PHYSICAL,
                label);
    } else {
        // Currently only the Beckmann specular model is supported for BTDFs
        if (isZero(anisotropy)) {
            MicrofacetIsotropicBTDF dielectricBTDF;
            MicrofacetIsotropicBTDF_init(dielectricBTDF,
                    N,
                    refrIor,
                    transRoughnessClamped,
                    transmissionColor,
                    abbeNumber,
                    specularModel,
                    MICROFACET_GEOMETRIC_SMITH);

            BsdfBuilder_addMicrofacetIsotropicBTDF(
                    bsdfBuilder,
                    dielectricBTDF,
                    transmissionWeight,
                    BSDFBUILDER_PHYSICAL,
                    label);
        } else {
            const varying Vec2f rgh = computeAnisoRoughness(transRoughness,
                                                            minRoughness,
                                                            anisotropy);

            // Currently there is no implementation for an anisotropic BTDF,
            // this will fallback to an isotropic BTDF
            MicrofacetAnisotropicBTDF dielectricBTDF;
            MicrofacetAnisotropicBTDF_init(dielectricBTDF,
                    N,
                    refrIor,
                    rgh.x, rgh.y,
                    shadingTangent,
                    transmissionColor,
                    abbeNumber,
                    specularModel,
                    MICROFACET_GEOMETRIC_SMITH);

            BsdfBuilder_addMicrofacetAnisotropicBTDF(
                    bsdfBuilder,
                    dielectricBTDF,
                    transmissionWeight,
                    BSDFBUILDER_PHYSICAL,
                    label);
        }
    }
}

void
DWABASE_createLobes(const uniform Material * uniform me,
                    uniform ShadingTLState * uniform tls,
                    const varying State &state,
                    varying BsdfBuilder &bsdfBuilder,
                    const varying DwaBaseParameters * uniform params,
                    const uniform DwaBaseUniformParameters * uniform uParams)
{
    // print out the parameters for debugging
    // DWABASELAYERABLE_printParameters(params, uParams);

    const uniform DwaBaseLayerable * uniform dwaBaseLayerable =
            (const uniform DwaBaseLayerable* uniform)getISPCDwaBaseLayerablePtr(me);

    const DwaBaseEventMessages * uniform const eventMessages = dwaBaseLayerable->mEventMessagesPtr;

    const uniform DwaBaseLabels * uniform labels =
        (const uniform DwaBaseLabels* uniform) getLabelsDataPtr(me);

    // Emission
    BsdfBuilder_addEmission(bsdfBuilder, params->mEmission);

    const varying Vec3f& N = params->mNormal;

    if (uParams->mThinGeometry) {
        BsdfBuilder_setThinGeo(bsdfBuilder);
    }

    if (uParams->mPreventLightCulling) {
        BsdfBuilder_setPreventLightCulling(bsdfBuilder, true);
    }

    // Apply roughness clamping
    // FIXME: We should handle roughness clamping automatically
    // in the BsdfBuilder, rather than relying on the shader
    // writer to handle it explicitly
    // Get minimum roughness used to apply roughness clamping.
    const varying Vec2f tmp = getMinRoughness(state);
    const varying float minRoughness = min(tmp.x, tmp.y);

    // Fuzz
    if (!isZero(params->mFuzz) && isEntering(state) == true &&
            !isZero(params->mFuzzRoughness)) {

        // This mapping for roughness is for backwards compatibility
        // can be removed soon
        VelvetBRDF fuzz;
        VelvetBRDF_init(fuzz,
                        params->mFuzzNormal,
                        clamp(0.05f + params->mFuzzRoughness, 0.0f, 1.0f),
                        params->mFuzzAlbedo,
                        params->mFuzzUseAbsorbingFibers);

        BsdfBuilder_addVelvetBRDF(bsdfBuilder,
                                  fuzz,
                                  params->mFuzz,
                                  BSDFBUILDER_PHYSICAL,
                                  labels->mFuzz);
    }

    // setup iridescence if needed
    varying Iridescence * uniform iridescence = nullptr;
    const varying IridescenceParameters * uniform iridescenceParams =
            &params->mIridescenceParameters;
    // If any of the active lanes have non-zero iridescence, we need to allocate
    // and initialize the iridescence object.  Without the "any" we would
    // allocate the iridescence object, but the initialization would
    // be masked for the lanes with zero value iridescence.  The uninitialized
    // data would be read when the lobe is created and likely lead to a crash.
    if (any(!isZero(iridescenceParams->mIridescence))) {
        iridescence = (varying Iridescence * uniform)
             Arena_alloc(tls->mArena, sizeof(Iridescence));
        if (iridescenceParams->mIridescenceColorControl == SHADING_IRIDESCENCE_COLOR_USE_HUE_INTERPOLATION) {
            Iridescence_init(*iridescence,
                             N,
                             iridescenceParams->mIridescence,
                             iridescenceParams->mIridescencePrimaryColor,
                             iridescenceParams->mIridescenceSecondaryColor,
                             iridescenceParams->mIridescenceFlipHueDirection,
                             iridescenceParams->mIridescenceThickness,
                             iridescenceParams->mIridescenceExponent,
                             iridescenceParams->mIridescenceAt0,
                             iridescenceParams->mIridescenceAt90);
        } else { // SHADING_IRIDESCENCE_COLOR_USE_RAMP
            Iridescence_init(*iridescence,
                             N,
                             iridescenceParams->mIridescence,
                             iridescenceParams->mIridescenceRampInterpolationMode,
                             iridescenceParams->mIridescenceRampNumPoints,
                             iridescenceParams->mIridescenceRampPositions,
                             iridescenceParams->mIridescenceRampInterpolators,
                             iridescenceParams->mIridescenceRampColors,
                             iridescenceParams->mIridescenceThickness,
                             iridescenceParams->mIridescenceExponent,
                             iridescenceParams->mIridescenceAt0,
                             iridescenceParams->mIridescenceAt90);
        }
    }

    varying Iridescence * uniform outerIridescence = nullptr;
    varying Iridescence * uniform primaryIridescence = nullptr;
    if (iridescenceParams->mIridescenceApplyTo == IRIDESCENCE_OUTER_SPECULAR) {
        outerIridescence = iridescence;
    } else {
        primaryIridescence = iridescence;
    }

    // Outer specular reflection/clearcoat (ignored when exiting)
    if (!isZero(params->mOuterSpecular) && isEntering(state) == true) {
        DWABASE_addOuterSpecularLobes(bsdfBuilder, params, uParams, labels, minRoughness, outerIridescence);
    }

    if (!isZero(params->mGlitterVaryingParameters.mGlitterMask) == true) {
        uniform bool exitEarly = false;
        DWABASE_addGlitterLobes(me, bsdfBuilder, tls, state, params, labels, exitEarly, eventMessages);
        if (exitEarly) return;
    }

    // Fabric and velvet lobes
    {
        BsdfBuilder_startAdjacentComponents(bsdfBuilder);

        const varying float warpRoughness = max(params->mWarpRoughness, minRoughness);
        const varying float weftRoughness = max(params->mWeftRoughness, minRoughness);

        const varying Color& warpColor = params->mWarpColor;
        if (!isBlack(warpColor)) {
            FabricBRDF warp;
            FabricBRDF_init(warp, N,
                            params->mFabricTangent,
                            params->mWarpThreadDirection,
                            params->mWarpThreadElevation,
                            warpRoughness,
                            warpColor);

            BsdfBuilder_addFabricBRDF(bsdfBuilder, warp,
                    params->mWarpThreadCoverage * params->mFabricSpecular,
                    BSDFBUILDER_PHYSICAL,
                    labels->mSpecular);
        }

        const varying Color& weftColor = params->mWeftColor;
        if (!isBlack(weftColor)) {
            // Rotate the warp thread by 90 in (X,Y)
            const varying Vec3f weftThreadDirection = {-params->mWarpThreadDirection.y,
                                                        params->mWarpThreadDirection.x,
                                                        params->mWarpThreadDirection.z};

            FabricBRDF weft;
            FabricBRDF_init(weft, N,
                            params->mFabricTangent,
                            weftThreadDirection,
                            0.f, // no elevation on the second (weft) thread
                            weftRoughness,
                            weftColor);

            BsdfBuilder_addFabricBRDF(bsdfBuilder, weft,
                    (1.f - params->mWarpThreadCoverage) * params->mFabricSpecular,
                    BSDFBUILDER_PHYSICAL,
                    labels->mSpecular);
        }

        BsdfBuilder_endAdjacentComponents(bsdfBuilder);
    }

    // For internal reflections from a refracted ray, if the
    // "use independent transmission refractive index" parameter
    // is true, we use the "independent transmission refractive index".
    // Not doing this results in incorrect TIR artifacts.
    varying float reflIor = params->mRefractiveIndex;
    if (params->mUseIndependentTransmissionRefractiveIndex && !isEntering(state)) {
        reflIor = params->mIndependentTransmissionRefractiveIndex;
    }

    // Hair toon specular 1
    if (!isZero(params->mHairToonS1Params.mToonSpecular)) {
        if (uParams->mHairToonS1Model == ToonSpecularSurface) {
            DWABASE_addToonSpecularLobes(bsdfBuilder,
                                         params->mHairToonS1Params,
                                         labels);
        } else if (uParams->mHairToonS1Model == ToonSpecularHair) {
            DWABASE_addHairToonSpecularLobes(bsdfBuilder,
                                             params->mHairToonS1Params,
                                             labels);
        }
    }

    if (!isZero(params->mHairToonS2Params.mToonSpecular)) {
        if (uParams->mHairToonS2Model == ToonSpecularSurface) {
            DWABASE_addToonSpecularLobes(bsdfBuilder,
                                         params->mHairToonS2Params,
                                         labels);
        } else if (uParams->mHairToonS2Model == ToonSpecularHair) {
            DWABASE_addHairToonSpecularLobes(bsdfBuilder,
                                             params->mHairToonS2Params,
                                             labels);
        }
    }

    if (!isZero(params->mHairToonS3Params.mToonSpecular)) {
        if (uParams->mHairToonS3Model == ToonSpecularSurface) {
            DWABASE_addToonSpecularLobes(bsdfBuilder,
                                         params->mHairToonS3Params,
                                         labels);
        } else if (uParams->mHairToonS3Model == ToonSpecularHair) {
            DWABASE_addHairToonSpecularLobes(bsdfBuilder,
                                             params->mHairToonS3Params,
                                             labels);
        }
    }

    // Non-hair toon specular
    if (!isZero(params->mToonSpecularParams.mToonSpecular) &&
        uParams->mToonSpecularModel == ToonSpecularSurface) {

        const ToonSpecularParameters& toonParams = params->mToonSpecularParams;

        DWABASE_addToonSpecularLobes(bsdfBuilder,
                                     toonParams,
                                     labels);

        // Transmission
        if (!isZero(params->mTransmission)) { 

            const float roughnessClamped = max(params->mRoughness, minRoughness);

            float refrIor = reflIor;
            if (params->mUseIndependentTransmissionRefractiveIndex) {
                refrIor = params->mIndependentTransmissionRefractiveIndex;
            }

            // Dispersion
            // Only allow for primary ray intersections to safeguard against splitting a ray twice.
            // Ideally, we tag the ray with a wavelength it carries or whether it has been split already.
            const varying float abbeNumber = (isIndirect(state)) ? 0.0f : params->mDispersionAbbeNumber;

            if (roughnessClamped < sEpsilon) {
                MirrorBTDF mirrorBTDF;
                MirrorBTDF_init(mirrorBTDF,
                                N,
                                refrIor,
                                params->mTransmissionColor,
                                abbeNumber);
                BsdfBuilder_addMirrorBTDF(bsdfBuilder,
                                          mirrorBTDF,
                                          params->mTransmission,
                                          BSDFBUILDER_PHYSICAL,
                                          labels->mSpecularTransmission);
            } else {
                MicrofacetIsotropicBTDF dielectricBTDF;
                MicrofacetIsotropicBTDF_init(dielectricBTDF,
                                             N,
                                             refrIor,
                                             roughnessClamped,
                                             params->mTransmissionColor,
                                             abbeNumber,
                                             (MicrofacetDistribution) uParams->mSpecularModel,
                                             MICROFACET_GEOMETRIC_SMITH);

                BsdfBuilder_addMicrofacetIsotropicBTDF(bsdfBuilder,
                                                       dielectricBTDF,
                                                       params->mTransmission,
                                                       BSDFBUILDER_PHYSICAL,
                                                       labels->mSpecularTransmission);
            }
        }
    } else if (!isZero(params->mSpecular) || !isZero(params->mTransmission)) {
        // Specular reflection and transmission
        // Modulate roughness based on normal map mip-map anti-aliasing strategy.
        const varying float roughness = computeMicrofacetRoughness(
                                            params->mRoughness,
                                            params->mNormalAAStrategy,
                                            params->mNormalLength,
                                            params->mNormalAADial,
                                            params->mNormalDial);

        // setup shading tangent if needed
        varying Vec3f shadingTangent = Vec3f_ctor(0.f);
        if (!isZero(params->mAnisotropy)) {
            const varying Vec3f T = normalize(getdPds(state));
            varying ReferenceFrame frame;
            ReferenceFrame_init(frame, N, T);
            const varying Vec2f& shadingTangentLocal = params->mShadingTangent;
            shadingTangent = localToGlobal(frame,
                    normalize(Vec3f_ctor(shadingTangentLocal.x,
                                         shadingTangentLocal.y,
                                         0.f)));
        }

        // Metallic and dielectric specular reflection lobes
        const varying float roughnessClamped = max(roughness, minRoughness);

        // Metallic specular reflection lobe
        const varying float metallicWeight = params->mSpecular * params->mMetallic;

        if (!isZero(metallicWeight)) {
            if (roughnessClamped < sEpsilon) {
                MirrorBRDF conductorBRDF;
                MirrorBRDF_init(conductorBRDF,
                                N,
                                params->mMetallicColor,
                                params->mMetallicEdgeColor,
                                primaryIridescence);

                BsdfBuilder_addMirrorBRDF(bsdfBuilder, conductorBRDF,
                        metallicWeight,
                        BSDFBUILDER_PHYSICAL,
                        labels->mSpecular);

            } else {
                if (isZero(params->mAnisotropy)) {
                    MicrofacetIsotropicBRDF conductorBRDF;
                    MicrofacetIsotropicBRDF_init(conductorBRDF,
                            N,
                            params->mMetallicColor,
                            params->mMetallicEdgeColor,
                            roughnessClamped,
                            (MicrofacetDistribution) uParams->mSpecularModel,
                            MICROFACET_GEOMETRIC_SMITH,
                            primaryIridescence);

                    BsdfBuilder_addMicrofacetIsotropicBRDF(
                            bsdfBuilder,
                            conductorBRDF,
                            metallicWeight,
                            BSDFBUILDER_PHYSICAL,
                            labels->mSpecular);
                } else {
                    const varying Vec2f rgh = computeAnisoRoughness(roughness,
                                                                    minRoughness,
                                                                    params->mAnisotropy);

                    MicrofacetAnisotropicBRDF conductorBRDF;
                    MicrofacetAnisotropicBRDF_init(conductorBRDF,
                            N,
                            params->mMetallicColor,
                            params->mMetallicEdgeColor,
                            rgh.x, rgh.y,
                            shadingTangent,
                            (MicrofacetDistribution) uParams->mSpecularModel,
                            MICROFACET_GEOMETRIC_SMITH,
                            primaryIridescence);

                    BsdfBuilder_addMicrofacetAnisotropicBRDF(
                            bsdfBuilder,
                            conductorBRDF,
                            metallicWeight,
                            BSDFBUILDER_PHYSICAL,
                            labels->mSpecular);
                }
            }
        }

        // if this material is fully metallic we can exit early
        if (isOne(metallicWeight)) {
            return;
        }

        const varying float dielectricWeight = params->mSpecular;

        // Coupled dielectric specular reflection and transmission lobes
        float refrIor = reflIor;

        if (params->mUseIndependentTransmissionRefractiveIndex) {
            refrIor = params->mIndependentTransmissionRefractiveIndex;
        }

        // Dispersion
        // Only allow for primary ray intersections to safeguard against splitting a ray twice.
        // Ideally, we tag the ray with a wavelength it carries or whether it has been split already.
        const varying float abbeNumber = (isIndirect(state)) ? 0.0f : params->mDispersionAbbeNumber;

        if (params->mUseIndependentTransmissionRoughness) {
            DWABASE_addIndependentReflectionLobe(bsdfBuilder,
                                                 N, reflIor,
                                                 params->mAnisotropy, roughness, minRoughness, shadingTangent,
                                                 (MicrofacetDistribution) uParams->mSpecularModel,
                                                 dielectricWeight,
                                                 primaryIridescence,
                                                 labels->mSpecular);

            DWABASE_addIndependentTransmissionLobe(bsdfBuilder,
                                                   N, refrIor,
                                                   params->mAnisotropy,
                                                   params->mIndependentTransmissionRoughness,
                                                   minRoughness,
                                                   shadingTangent,
                                                   (MicrofacetDistribution) uParams->mSpecularModel,
                                                   params->mTransmissionColor,
                                                   abbeNumber,
                                                   params->mTransmission,
                                                   labels->mSpecularTransmission);
        } else {
            if (roughnessClamped < sEpsilon) {
                MirrorBSDF dielectricBSDF;
                MirrorBSDF_init(dielectricBSDF,
                                N,
                                reflIor,
                                params->mTransmissionColor,
                                abbeNumber,
                                refrIor,
                                dielectricWeight,
                                params->mTransmission,
                                primaryIridescence);

                BsdfBuilder_addMirrorBSDF(bsdfBuilder,
                                          dielectricBSDF,
                                          1.f, // weight
                                          BSDFBUILDER_PHYSICAL,
                                          labels->mSpecular,
                                          labels->mSpecularTransmission);
            } else {
                if (isZero(params->mAnisotropy)) {
                    MicrofacetIsotropicBSDF dielectricBSDF;
                    MicrofacetIsotropicBSDF_init(dielectricBSDF,
                            N,
                            reflIor,
                            roughnessClamped,
                            (MicrofacetDistribution) uParams->mSpecularModel,
                            MICROFACET_GEOMETRIC_SMITH,
                            params->mTransmissionColor,
                            abbeNumber,
                            refrIor,
                            dielectricWeight,
                            params->mTransmission,
                            primaryIridescence);

                    BsdfBuilder_addMicrofacetIsotropicBSDF(
                            bsdfBuilder,
                            dielectricBSDF,
                            1.f, // weight
                            BSDFBUILDER_PHYSICAL,
                            labels->mSpecular,
                            labels->mSpecularTransmission);
                } else {
                    const Vec2f rgh =
                        computeAnisoRoughness(roughness, minRoughness, params->mAnisotropy);

                    MicrofacetAnisotropicBSDF dielectricBSDF;
                    MicrofacetAnisotropicBSDF_init(dielectricBSDF,
                            N,
                            reflIor,
                            rgh.x, rgh.y,
                            shadingTangent,
                            (MicrofacetDistribution) uParams->mSpecularModel,
                            MICROFACET_GEOMETRIC_SMITH,
                            params->mTransmissionColor,
                            abbeNumber,
                            refrIor,
                            dielectricWeight,
                            params->mTransmission,
                            primaryIridescence);

                    BsdfBuilder_addMicrofacetAnisotropicBSDF(
                            bsdfBuilder,
                            dielectricBSDF,
                            1.f,
                            BSDFBUILDER_PHYSICAL,
                            labels->mSpecular,
                            labels->mSpecularTransmission);
                }
            }
        }
    }

    // Diffuse lobes
    const varying bool zeroScatterRadius = isBlack(params->mScatteringRadius);
    const varying Color diffuseTransmissionAttenuation = DWABASE_getDiffuseTransmissionAttenuation(params);
    const varying Color reflectionAlbedo = params->mAlbedo * diffuseTransmissionAttenuation;
    const varying bool blackAlbedo = isBlack(reflectionAlbedo);

    // Toon Ramp Diffuse
    if (!blackAlbedo) {
        // If there is any SSS, toon diffuse is totally ignored
        // otherwise, toon ramp is applied before other diffuse models
        if (zeroScatterRadius && !isZero(params->mToonDiffuseParams.mToonDiffuse) &&
            params->mToonDiffuseParams.mModel == TOON_DIFFUSE_RAMP) {

            ToonBRDF toon;
            ToonBRDF_init(toon,
                          params->mToonDiffuseParams.mNormal,
                          reflectionAlbedo,
                          params->mToonDiffuseParams.mRampNumPoints,
                          params->mToonDiffuseParams.mRampPositions,
                          params->mToonDiffuseParams.mRampInterpolators,
                          params->mToonDiffuseParams.mRampColors,
                          params->mToonDiffuseParams.mExtendRamp);

            BsdfBuilder_addToonBRDF(bsdfBuilder,
                                    toon,
                                    params->mFabricAttenuation * params->mToonDiffuseParams.mToonDiffuse,
                                    BSDFBUILDER_PHYSICAL,
                                    labels->mDiffuse);
        }
    }

    // Oren-Nayar and Subsurface Scattering Diffuse
    BsdfBuilder_startAdjacentComponents(bsdfBuilder);
    if (!blackAlbedo) {
        if (zeroScatterRadius) {
            // shading normal in most cases will be N,
            // but when blending toon materials it may be intentionally out of surface hemisphere
            // in this case light culling will be off, so we use that for all surface diffuse
            const varying Vec3f shadingN = uParams->mPreventLightCulling ?
                                           params->mToonDiffuseParams.mNormal : params->mNormal;

            // this param is blended only between toons so we need to weight it by what % of the layered material is toon    
            const float flatness = params->mToonDiffuseParams.mFlatness * params->mToonDiffuseParams.mToonDiffuse;

            // flat diffuse is oren-nayar with an optional NPR parameter (flatness)
            FlatDiffuseBRDF diffuseRefl;
            FlatDiffuseBRDF_init(diffuseRefl,
                                 shadingN,
                                 reflectionAlbedo,
                                 params->mDiffuseRoughness,
                                 params->mToonDiffuseParams.mTerminatorShift,
                                 flatness,
                                 params->mToonDiffuseParams.mFlatnessFalloff);

            BsdfBuilder_addFlatDiffuseBRDF(bsdfBuilder, diffuseRefl,
                                           params->mFabricAttenuation,
                                           BSDFBUILDER_PHYSICAL,
                                           labels->mDiffuse);
        } else {
            const SubsurfaceType bssrdfType = (SubsurfaceType)uParams->mSubsurface;
            if (bssrdfType == SUBSURFACE_NORMALIZED_DIFFUSION) {
                NormalizedDiffusion diffuseRefl;
                NormalizedDiffusion_init(diffuseRefl,
                        N,
                        reflectionAlbedo,
                        params->mScatteringRadius,
                        me,
                        params->mSubsurfaceTraceSet,
                        params->mEvalSubsurfaceNormalFn);

                BsdfBuilder_addNormalizedDiffusion(bsdfBuilder, diffuseRefl,
                        params->mFabricAttenuation,
                        BSDFBUILDER_PHYSICAL,
                        labels->mDiffuse);

            } else if (bssrdfType == SUBSURFACE_DIPOLE_DIFFUSION) {
                DipoleDiffusion diffuseRefl;
                DipoleDiffusion_init( diffuseRefl,
                        N,
                        reflectionAlbedo,
                        params->mScatteringRadius,
                        me,
                        params->mSubsurfaceTraceSet,
                        params->mEvalSubsurfaceNormalFn);

                BsdfBuilder_addDipoleDiffusion(bsdfBuilder, diffuseRefl,
                        params->mFabricAttenuation,
                        BSDFBUILDER_PHYSICAL,
                        labels->mDiffuse);

            } else if (bssrdfType == SUBSURFACE_RANDOM_WALK) {
                RandomWalkSubsurface rwSubsurface;
                RandomWalkSubsurface_init(rwSubsurface,
                        N,
                        reflectionAlbedo,
                        params->mScatteringRadius,
                        params->mSSSResolveSelfIntersections,
                        me,
                        params->mSubsurfaceTraceSet,
                        params->mEvalSubsurfaceNormalFn
                        );

                BsdfBuilder_addRandomWalkSubsurface(bsdfBuilder, rwSubsurface,
                        params->mFabricAttenuation,
                        BSDFBUILDER_PHYSICAL,
                        labels->mDiffuse);
            }
        }
    }

    // Translucent Diffuse lobe
    if (!isBlack(params->mDiffuseTransmission)) {

        LambertianBTDF diffuseTrans;
        LambertianBTDF_init(diffuseTrans,
                neg(N),
                params->mDiffuseTransmission);

        BsdfBuilder_addLambertianBTDF(bsdfBuilder, diffuseTrans,
                params->mFabricAttenuation,
                BSDFBUILDER_PHYSICAL,
                labels->mDiffuseTransmission);
    }

    BsdfBuilder_endAdjacentComponents(bsdfBuilder);

    // Add Hair Lobes
    DWABASE_addHairLobes(me, tls, state, bsdfBuilder, params, uParams, labels, eventMessages);
}

void
DWABASELAYERABLE_initGlitterVaryingParameters(varying GLITTER_VaryingParameters * uniform varyingParams)
{
    varyingParams->mFlakeStyleFrequency[0] = 1.0f;
    varyingParams->mFlakeColor[0] = sWhite;
    varyingParams->mFlakeSize[0] = 1.0f;
    varyingParams->mFlakeRoughness[0] = 0.14f;
    varyingParams->mFlakeStyleFrequency[1] = 0.0f;
    varyingParams->mFlakeColor[1] = sWhite;
    varyingParams->mFlakeSize[1] = 1.0f;
    varyingParams->mFlakeRoughness[1] = 0.14f;
    varyingParams->mFlakeHSVColorVariation = Vec3f_ctor(0.0f, 0.0f, 0.0f);
    varyingParams->mFlakeDensity = 1.0f;;
    varyingParams->mFlakeJitter = 1.0f;
    varyingParams->mFlakeOrientationRandomness = 0.15f;
    varyingParams->mCompensateDeformation = true;
    varyingParams->mApproximateForSecRays = true;
    varyingParams->mGlitterMask = 0.0f;
}

void
DWABASELAYERABLE_initIridescenceParameters(varying IridescenceParameters * uniform params)
{
    params->mIridescence = 0.0f;
    params->mIridescenceApplyTo = IRIDESCENCE_PRIMARY_SPECULAR;
    params->mIridescenceColorControl = SHADING_IRIDESCENCE_COLOR_USE_HUE_INTERPOLATION;
    params->mIridescencePrimaryColor = sWhite;
    params->mIridescenceSecondaryColor = sWhite;
    params->mIridescenceFlipHueDirection = false;
    params->mIridescenceThickness = 0.0f;
    params->mIridescenceExponent = 1.0f;
    params->mIridescenceAt0 = 0.0f;
    params->mIridescenceAt90 = 0.0f;
    params->mIridescenceRampInterpolationMode = COLOR_RAMP_CONTROL_SPACE_RGB;
    params->mIridescenceRampNumPoints = 1;
    params->mIridescenceRampPositions[0] = 0.0f;
    params->mIridescenceRampColors[0] = sWhite;
    params->mIridescenceRampInterpolators[0] = RAMP_INTERPOLATOR_MODE_NONE;
}

void
DWABASELAYERABLE_initHairParameters(varying HairParameters * uniform params)
{
    // Common Hair params
    params->mHairDir = Vec3f_ctor(0.0f, 0.0f, 0.0f);
    params->mHairColor = Color_ctor(1.0f, 1.0f, 1.0f);

    // HairMaterial_v3 params
    params->mHair = 0.0f;
    params->mHairCastsCaustics = false;
    params->mHairUV = Vec2f_ctor(0.0f, 0.0f);
    params->mHairIOR = 1.45f;
    params->mHairShowR = true;
    params->mHairRShift = 0.0f;
    params->mHairRLongRoughness = 0.5f;
    params->mHairRTint = Color_ctor(1.0f, 1.0f, 1.0f);
    params->mHairShowTT = true;
    params->mHairTTShift = 0.0f;
    params->mHairTTLongRoughness = 0.1f;
    params->mHairTTAzimRoughness = 1.0f;
    params->mHairTTTint = Color_ctor(1.0f, 1.0f, 1.0f);
    params->mHairShowTRT = true;
    params->mHairTRTShift = 0.0f;
    params->mHairTRTLongRoughness = 0.4f;
    params->mHairTRTTint = Color_ctor(1.0f, 1.0f, 1.0f);
    params->mHairShowGlint = false;
    params->mHairGlintRoughness = 0.5f;
    params->mHairGlintMinTwists = 1.5f;
    params->mHairGlintMaxTwists = 2.5f;
    params->mHairGlintEccentricity = 0.85f;
    params->mHairGlintSaturation = 0.5f;
    params->mHairShowTRRT = true;
    params->mHairTRRTLongRoughness = 4.0f * params->mHairRLongRoughness;
    params->mHairFresnelType = HAIR_FRESNEL_DIELECTRIC_CYLINDER;
    params->mHairCuticleLayerThickness = 0.1f;
    params->mHairUseOptimizedSampling = true;

    // HairDiffuseMaterial params
    params->mHairDiffuse = 0.0f;
    params->mHairDiffuseUseIndependentFrontAndBackColor = false;
    params->mHairDiffuseFrontColor = Color_ctor(1.0f, 1.0f, 1.0f);
    params->mHairDiffuseBackColor = Color_ctor(1.0f, 1.0f, 1.0f);

    // experimental
    params->mHairSubsurfaceBlend = 1.0f;
}

void
DWABASELAYERABLE_initToonSpecularParameters(varying ToonSpecularParameters * uniform params)
{
    params->mToonSpecular = 0.0f;
    params->mIntensity = 1.0f;
    params->mRoughness = 0.9f;
    params->mTint = Color_ctor(1.0f, 1.0f, 1.0f);
    params->mNormal = Vec3f_ctor(0.0f, 0.0f, 0.0f);
    params->mRampNumPoints = 1;
    params->mRampPositions[0] = 0.0f;
    params->mRampValues[0] = 1.0f;
    params->mRampInterpolators[0] = RAMP_INTERPOLATOR_MODE_NONE;
    params->mNormal = Vec3f_ctor(0.0f, 0.0f, 0.0f);
    params->mStretchU = 1.0f;
    params->mStretchV = 1.0f;
    params->mdPds = Vec3f_ctor(0.0f, 0.0f, 0.0f);
    params->mdPdt = Vec3f_ctor(0.0f, 0.0f, 0.0f);
    params->mEnableIndirectReflections = false;
    params->mIndirectReflectionsIntensity = 0.0f;
    params->mIndirectReflectionsRoughness = 0.5f;

    params->mHairDir = Vec3f_ctor(0.0f, 0.0f, 0.0f);
    params->mHairUV = Vec2f_ctor(0.0f, 0.0f);
    params->mHairIOR = 1.45f;
    params->mHairFresnelType = HAIR_FRESNEL_DIELECTRIC_CYLINDER;
    params->mHairCuticleLayerThickness = 0.1f;
    params->mHairRShift = 0.0f;
}

void
DWABASELAYERABLE_initToonDiffuseParameters(varying ToonDiffuseParameters * uniform params)
{
    params->mToonDiffuse = 0.0f;
    params->mModel = TOON_DIFFUSE_OREN_NAYAR;
    params->mTerminatorShift = 0.0f;
    params->mFlatness = 0.0f;
    params->mFlatnessFalloff = 0.0f;
    params->mRampNumPoints = 1;
    params->mRampPositions[0] = 0.0f;
    params->mRampColors[0] = sWhite;
    params->mRampInterpolators[0] = RAMP_INTERPOLATOR_MODE_NONE;
    params->mNormal = Vec3f_ctor(0.0f, 0.0f, 0.0f);
    params->mExtendRamp = false;
}

void
DWABASELAYERABLE_initParameters(varying DwaBaseParameters * uniform params)
{
    // The following uniform params are set in update:
    // mOuterSpecularModel
    // mOuterSpecularUseBending
    // mSpecularModel
    // mSubsurface
    // mThinGeometry
    // mPreventLightCulling

    // Glitter params
    params->mGlitterUniformParameters = nullptr;
    DWABASELAYERABLE_initGlitterVaryingParameters(&params->mGlitterVaryingParameters);

    // Hair params
    DWABASELAYERABLE_initHairParameters(&params->mHairParameters);

    // Hair Toon params
    DWABASELAYERABLE_initToonSpecularParameters(&params->mHairToonS1Params);
    DWABASELAYERABLE_initToonSpecularParameters(&params->mHairToonS2Params);
    DWABASELAYERABLE_initToonSpecularParameters(&params->mHairToonS3Params);

    // Fuzz params
    params->mFuzz = 0.0f;
    params->mFuzzRoughness = 0.0f;
    params->mFuzzAlbedo = sWhite;
    params->mFuzzUseAbsorbingFibers = false;
    params->mFuzzNormal = Vec3f_ctor(0.0f, 0.0f, 0.0f);

    // OuterSpecular params
    params->mOuterSpecular = 0.0f;
    params->mOuterSpecularRefractiveIndex = 1.5f;
    params->mOuterSpecularRoughness = 0.0f;
    params->mOuterSpecularThickness = 0.0f;
    params->mOuterSpecularAttenuationColor = sWhite;
    params->mOuterSpecularNormal = Vec3f_ctor(0.0f, 0.0f, 0.0f);
    params->mOuterSpecularNormalLength = 1.0f;
    params->mOuterSpecularNormalDial = 1.0f;

    // metallic params
    params->mMetallic = 0.0f;
    params->mMetallicColor = sWhite;
    params->mMetallicEdgeColor = sWhite;

    // specular params
    params->mSpecular = 0.0f;
    params->mRefractiveIndex = 1.5f;
    params->mRoughness = 0.0f;
    params->mAnisotropy = 0.0f;
    params->mShadingTangent = Vec2f_ctor(0.0f, 0.0f);

    // fabric params
    params->mFabricTangent = Vec3f_ctor(1.0f, 0.0f, 0.0f);
    params->mFabricSpecular = 0.0f;
    params->mWarpColor = sBlack;
    params->mWarpRoughness = 0.0f;
    params->mUseIndependentWeftAttributes = false;
    params->mWeftRoughness = 0.0f;
    params->mWeftColor = sBlack;
    params->mWarpThreadDirection = Vec3f_ctor(0.0f, 0.0f, 0.0f);
    params->mWarpThreadCoverage = 0.0f;
    params->mWarpThreadElevation = 0.0f;
    params->mFabricAttenuation = 1.0f;

    // iridescence params
    DWABASELAYERABLE_initIridescenceParameters(&params->mIridescenceParameters);

    // transmission params
    params->mTransmission = 0.0f;
    params->mTransmissionColor = sBlack;
    params->mUseIndependentTransmissionRefractiveIndex = false;
    params->mIndependentTransmissionRefractiveIndex = 1.5f;
    params->mUseIndependentTransmissionRoughness = false;
    params->mIndependentTransmissionRoughness = 0.0f;
    params->mDispersionAbbeNumber = 0.0f;

    // Toon params
    DWABASELAYERABLE_initToonDiffuseParameters(&params->mToonDiffuseParams);
    DWABASELAYERABLE_initToonSpecularParameters(&params->mToonSpecularParams);

    // diffuse params
    params->mAlbedo = sBlack;
    params->mDiffuseRoughness = 0.0f;
    params->mScatteringRadius = sBlack;

    params->mDiffuseTransmission = sBlack;
    params->mDiffuseTransmissionBlendingBehavior = DIFFUSE_TRANSMISSION_BLENDING_MONOCHROMATIC;
    params->mSubsurfaceTraceSet = 0;
    params->mSSSResolveSelfIntersections = true;

    // other params
    params->mNormal = Vec3f_ctor(0.0f, 0.0f, 0.0f);
    params->mNormalDial = 1.0f;
    params->mNormalLength = 1.0f;
    params->mNormalAAStrategy = NORMAL_AA_STRATEGY_NONE;
    params->mNormalAADial = 1.0f;
    params->mEmission = sBlack;
    params->mEvalSubsurfaceNormalFn = 0;
}

