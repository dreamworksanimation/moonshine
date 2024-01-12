// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

///
/// @file DwaBase.h
/// $Id$
///

#pragma once

#include "DwaBaseLayerable.h"
#include "DwaBase_ispc_stubs.h"

#include <moonray/rendering/shading/MaterialApi.h>
#include <moonshine/material/glitter/Glitter.h>
#include <moonray/rendering/bvh/shading/Xform.h>
#include <scene_rdl2/scene/rdl2/rdl2.h>

namespace moonshine {
namespace dwabase {

// Add all the labels emitted by the Dwa* material system here.
// These must correspond to the labels declared in labels.json.
// These should be declared in every Dwa* material .cc file.
#define DECLARE_DWA_BASE_LABELS()    \
    static const ispc::DwaBaseLabels \
    sLabels = {                      \
      aovHair,                       \
      aovHairR,                      \
      aovHairTT,                     \
      aovHairTRT,                    \
      aovHairTRRT,                   \
      aovHairDiffuse,                \
      aovFuzz,                       \
      aovOuterSpecular,              \
      aovSpecular,                   \
      aovDiffuse,                    \
      aovSpecularTransmission,       \
      aovDiffuseTransmission,        \
      aovGlitter                     \
  };

#define ASSIGN_GLITTER_ATTR_KEYS(keys) \
    keys.mShowGlitter                        = attrShowGlitter;                         \
    keys.mGlitterSeed                        = attrGlitterSeed;                         \
    keys.mGlitterSpace                       = attrGlitterSpace;                        \
    keys.mGlitterRandomness                  = attrGlitterRandomness;                   \
    keys.mGlitterDenseLodQuality             = attrGlitterLodQuality;                   \
    keys.mGlitterLayeringMode                = attrGlitterLayeringMode;                 \
    keys.mGlitterFlakeTextureA               = attrGlitterTextureA;                     \
    keys.mGlitterFlakeTextureB               = attrGlitterTextureB;                     \
    keys.mGlitter                            = attrGlitter;                             \
    keys.mGlitterDebugMode                   = attrGlitterDebugMode;                    \
    keys.mGlitterStyleAFrequency             = attrGlitterStyleAFrequency;              \
    keys.mGlitterFlakeColorA                 = attrGlitterColorA;                       \
    keys.mGlitterFlakeSizeA                  = attrGlitterSizeA;                        \
    keys.mGlitterFlakeRoughnessA             = attrGlitterRoughnessA;                   \
    keys.mGlitterStyleBFrequency             = attrGlitterStyleBFrequency;              \
    keys.mGlitterFlakeColorB                 = attrGlitterColorB;                       \
    keys.mGlitterFlakeSizeB                  = attrGlitterSizeB;                        \
    keys.mGlitterFlakeRoughnessB             = attrGlitterRoughnessB;                   \
    keys.mGlitterFlakeHueVariation           = attrGlitterHueVariation;                 \
    keys.mGlitterFlakeSaturationVariation    = attrGlitterSaturationVariation;          \
    keys.mGlitterFlakeValueVariation         = attrGlitterValueVariation;               \
    keys.mGlitterFlakeDensity                = attrGlitterDensity;                      \
    keys.mGlitterFlakeJitter                 = attrGlitterJitter;                       \
    keys.mGlitterFlakeOrientationRandomness  = attrGlitterTextureOrientationRandomness; \
    keys.mGlitterCompensateDeformation       = attrGlitterCompensateDeformation;        \
    keys.mGlitterApproximateForSecondaryRays = attrGlitterApproximateForSecRays;

#define ASSIGN_IRIDESCENCE_ATTR_KEYS(keys) \
    keys.mIridescence                       = attrIridescence;                      \
    keys.mIridescenceApplyTo                = attrIridescenceApplyTo;               \
    keys.mIridescenceColorControl           = attrIridescenceColorControl;          \
    keys.mIridescencePrimaryColor           = attrIridescencePrimaryColor;          \
    keys.mIridescenceSecondaryColor         = attrIridescenceSecondaryColor;        \
    keys.mIridescenceFlipHueDirection       = attrIridescenceFlipHueDirection;      \
    keys.mIridescenceThickness              = attrIridescenceThickness;             \
    keys.mIridescenceExponent               = attrIridescenceExponent;              \
    keys.mIridescenceAt0                    = attrIridescenceAt0;                   \
    keys.mIridescenceAt90                   = attrIridescenceAt90;                  \
    keys.mIridescenceRampInterpolationMode  = attrIridescenceRampInterpolationMode; \
    keys.mIridescenceRampInterpolators      = attrIridescenceRampInterpolators;     \
    keys.mIridescenceRampPositions          = attrIridescenceRampPositions;         \
    keys.mIridescenceRampColors             = attrIridescenceRampColors;

#define ASSIGN_TOON_DIFFUSE_ATTR_KEYS(keys) \
    keys.mToonDiffuseKeys.mInputNormal        = attrInputNormal;                  \
    keys.mToonDiffuseKeys.mInputNormalDial    = attrInputNormalDial;              \
    keys.mToonDiffuseKeys.mModel              = attrToonDiffuseModel;             \
    keys.mToonDiffuseKeys.mRampPositions      = attrToonDRampPositions;           \
    keys.mToonDiffuseKeys.mRampInterpolators  = attrToonDRampInterpolators;       \
    keys.mToonDiffuseKeys.mExtendRamp         = attrToonExtendRamp;               \
    keys.mToonDiffuseKeys.mTerminatorShift    = attrToonTerminatorShift;          \
    keys.mToonDiffuseKeys.mFlatness           = attrToonDiffuseFlatness;          \
    keys.mToonDiffuseKeys.mFlatnessFalloff    = attrToonDiffuseFlatnessFalloff;   

#define ASSIGN_TOON_SPEC_ATTR_KEYS(keys, name) \
    keys.m##name##Keys.mIntensity                  = attr##name##Intensity;                        \
    keys.m##name##Keys.mTint                       = attr##name##Tint;                             \
    keys.m##name##Keys.mRampInputScale             = attr##name##RampInputScale;                   \
    keys.m##name##Keys.mRampPositions              = attr##name##RampPositions;                    \
    keys.m##name##Keys.mRampValues                 = attr##name##RampValues;                       \
    keys.m##name##Keys.mRampInterpolators          = attr##name##RampInterpolators;                \
    keys.m##name##Keys.mEnableInputNormal          = attr##name##EnableInputNormal;                \
    keys.m##name##Keys.mInputNormal                = attr##name##InputNormal;                      \
    keys.m##name##Keys.mInputNormalDial            = attr##name##InputNormalDial;                  \
    keys.m##name##Keys.mStretchU                   = attr##name##StretchU;                         \
    keys.m##name##Keys.mStretchV                   = attr##name##StretchV;                         \
    keys.m##name##Keys.mUseInputVectors            = attr##name##UseInputVectors;                  \
    keys.m##name##Keys.mInputU                     = attr##name##InputU;                           \
    keys.m##name##Keys.mInputV                     = attr##name##InputV;                           \
    keys.m##name##Keys.mEnableIndirectReflections = attr##name##EnableIndirectReflections;         \
    keys.m##name##Keys.mIndirectReflectionsIntensity = attr##name##IndirectReflectionsIntensity;   \
    keys.m##name##Keys.mIndirectReflectionsRoughness = attr##name##IndirectReflectionsRoughness;

struct ToonDiffuseKeys
{
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::SceneObject *>      mInputNormal;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>              mInputNormalDial;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Int>                mModel;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>              mTerminatorShift;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>              mFlatness;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>              mFlatnessFalloff;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::IntVector>          mRampInterpolators;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::FloatVector>        mRampPositions;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>               mExtendRamp;
    std::vector<scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Rgb>>   mRampColors;
    std::vector<scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>> mRampOffsets;
};

struct ToonSpecularKeys
{
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>               mShow;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Int>                mModel;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>              mIntensity;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>              mRoughness;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Rgb>                mTint;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>              mRampInputScale;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::FloatVector>        mRampPositions;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::FloatVector>        mRampValues;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::IntVector>          mRampInterpolators;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>               mEnableInputNormal;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::SceneObject *>      mInputNormal;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>              mInputNormalDial;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>              mStretchU;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>              mStretchV;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>               mUseInputVectors;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Vec3f>              mInputU;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Vec3f>              mInputV;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>               mEnableIndirectReflections;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>              mIndirectReflectionsIntensity;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>              mIndirectReflectionsRoughness;
    std::vector<scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>> mRampMultipliers;
    std::vector<scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>> mRampOffsets;
};

// Each derived shader must pass an instance of DwaBaseAttributeKeys.
// The struct fields represent *all* supported attributes for DwaBase.
// A derived shader may choose to implement only some of the features
// DwaBase supports. If the derived shader does not support certain
// features, the struct fields associated with those features should
// be left unitialized.  DwaBase will check the validity of the
// AttributeKey before attempting to use it.
// For example, if the derived material does not support metallic specular,
// the struct fields associated with the metallic attributes should be
// left unset.
// If a feature is enabled (eg. via 'mShow*' attr key), then all of
// related AttributeKeys must also be set.  See DwaBase ctor for details.
struct DwaBaseAttributeKeys
{
    ToonDiffuseKeys mToonDiffuseKeys;
    ToonSpecularKeys mToonSpecularKeys;

    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mShowGlitter;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Int>     mGlitterSeed;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Int>     mGlitterSpace;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mGlitterRandomness;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mGlitterDenseLodQuality;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mGlitterDecoupleFlakeSize;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Int>     mGlitterLayeringMode;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::String>  mGlitterFlakeTextureA;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::String>  mGlitterFlakeTextureB;

    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mGlitter;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Int>     mGlitterDebugMode;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mGlitterStyleAFrequency;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Rgb>     mGlitterFlakeColorA;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mGlitterFlakeSizeA;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mGlitterFlakeRoughnessA;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mGlitterStyleBFrequency;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Rgb>     mGlitterFlakeColorB;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mGlitterFlakeSizeB;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mGlitterFlakeRoughnessB;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mGlitterFlakeHueVariation;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mGlitterFlakeSaturationVariation;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mGlitterFlakeValueVariation;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mGlitterFlakeDensity;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mGlitterFlakeJitter;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mGlitterFlakeOrientationRandomness;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mGlitterCompensateDeformation;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mGlitterApproximateForSecondaryRays;

    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Rgb>     mHairColor;

    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mShowHair;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mHair;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Int>     mHairFresnelType;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mHairCuticleLayerThickness;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mHairShowR;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mHairROffset;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mHairRRoughness;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Rgb>     mHairRTintColor;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mHairShowTRT;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mHairTRTOffset;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mHairTRTUseRoughness;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mHairTRTRoughness;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Rgb>     mHairTRTTintColor;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mHairShowGlint;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mHairGlintRoughness;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mHairGlintMinTwists;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mHairGlintMaxTwists;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mHairGlintEccentricity;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mHairGlintSaturation;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mHairShowTT;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mHairTTOffset;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mHairTTUseRoughness;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mHairTTRoughness;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mHairTTSaturation;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mHairTTAzimuthalRoughness;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Rgb>     mHairTTTintColor;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mHairUseOptimizedSampling;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mHairShowTRRTPlus;

    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mShowHairDiffuse;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mHairDiffuse;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mHairiDiffuseUseIndependentFrontAndBackColor;;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Rgb>     mHairDiffuseFrontColor;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Rgb>     mHairDiffuseBackColor;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mHairSubsurfaceBlend;

    ToonSpecularKeys mHairToonS1Keys;
    ToonSpecularKeys mHairToonS2Keys;
    ToonSpecularKeys mHairToonS3Keys;

    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mShowFuzz;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mFuzz;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Rgb>     mFuzzAlbedo;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mFuzzRoughness;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mFuzzUseAbsorbingFibers;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mUseFuzzNormal;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::SceneObject *> mFuzzNormal;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mFuzzNormalDial;

    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mShowOuterSpecular;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mOuterSpecular;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Int>     mOuterSpecularModel;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mOuterSpecularRefractiveIndex;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mOuterSpecularRoughness;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mOuterSpecularThickness;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Rgb>     mOuterSpecularAttenuationColor;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mOuterSpecularUseBending;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mUseOuterSpecularNormal;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::SceneObject *> mOuterSpecularNormal;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mOuterSpecularNormalDial;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mShowSpecular;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mSpecular;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Int>     mSpecularModel;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mRefractiveIndex;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mMetallic;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Rgb>     mMetallicColor;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Rgb>     mMetallicEdgeColor;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mRoughness;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mAnisotropy;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Vec2f>   mShadingTangent;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mIridescence;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Int>     mIridescenceApplyTo;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Int>     mIridescenceColorControl;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Rgb>     mIridescencePrimaryColor;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Rgb>     mIridescenceSecondaryColor;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mIridescenceFlipHueDirection;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mIridescenceThickness;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mIridescenceExponent;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mIridescenceAt0;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mIridescenceAt90;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Int>     mIridescenceRampInterpolationMode;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::IntVector>   mIridescenceRampInterpolators;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::FloatVector> mIridescenceRampPositions;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::RgbVector> mIridescenceRampColors;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mShowTransmission;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mTransmission;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Rgb>     mTransmissionColor;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mUseIndependentTransmissionRefractiveIndex;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mIndependentTransmissionRefractiveIndex;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mUseIndependentTransmissionRoughness;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mIndependentTransmissionRoughness;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mUseDispersion;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mDispersionAbbeNumber;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mShowDiffuse;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Rgb>     mAlbedo;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mDiffuseRoughness;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Int>     mSubsurface;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Rgb>     mScatteringColor;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mScatteringRadius;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::SceneObject *> mSubsurfaceTraceSet;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mEnableSubsurfaceInputNormal;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mSSSResolveSelfIntersections;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mDiffuseTransmission;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Rgb>     mDiffuseTransmissionColor;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Int>     mDiffuseTransmissionBlendingBehavior;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mShowFabricSpecular;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mFabricSpecular;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Rgb>     mWarpColor;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mWarpRoughness;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mUseIndependentWeftAttributes;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mWeftRoughness;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Rgb>     mWeftColor;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mUseUVsForThreadDirection;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Vec3f>   mWarpThreadDirection;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mWarpThreadCoverage;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mWarpThreadElevation;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mFabricDiffuseScattering;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mShowEmission;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Rgb>     mEmission;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mPresence;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::SceneObject *> mInputNormal;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mInputNormalDial;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mPreventLightCulling;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Int>     mNormalAAStrategy;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float>   mNormalAADial;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mThinGeometry;

    // required
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Bool>    mCastsCaustics;
};

// interface and helper class for layering DwaBase
class DwaBase : public DwaBaseLayerable
{

public:
    DwaBase(const scene_rdl2::rdl2::SceneClass& sceneClass,
            const std::string& name,
            const DwaBaseAttributeKeys& attrKeys,
            const ispc::DwaBaseAttributeFuncs& attrFuncs,
            const ispc::DwaBaseLabels& labels,
            const ispc::Model model);

    virtual ~DwaBase() = 0;

    int resolveSubsurfaceType(const moonray::shading::State& state) const override;

    void resolveUniformParameters(ispc::DwaBaseUniformParameters &uParams) const override;

    bool resolveParameters(moonray::shading::TLState *tls,
                           const moonray::shading::State& state,
                           bool castsCaustics,
                           ispc::DwaBaseParameters &params) const override;

    scene_rdl2::math::Vec3f resolveSubsurfaceNormal(moonray::shading::TLState *tls,
                                               const moonray::shading::State& state) const override;

    float resolvePresence(moonray::shading::TLState *tls,
                          const moonray::shading::State& state) const override;

    float resolveRefractiveIndex(moonray::shading::TLState *tls,
                                 const moonray::shading::State& state) const override;

    bool resolvePreventLightCulling(const moonray::shading::State& state) const override;

    void update() override;

    // Tests whether glitter is enabled for this material
    bool hasGlitter() const override;

    // Is glitter is enabled this gets the uniform parameters and
    // constructs a Glitter object from which we call createLobes
    // during shade.
    void updateGlitter();

    void updateIridescence();

    bool getCastsCaustics() const override
    {
        return mAttrKeys.mCastsCaustics.isValid() && get(mAttrKeys.mCastsCaustics);
    }

    bool getEnableSubsurfaceInputNormal() const
    {
        return get(mAttrKeys.mEnableSubsurfaceInputNormal);
    }

    scene_rdl2::rdl2::EvalNormalFunc getEvalSubsurfaceNormalFunc() const override
    {
        return reinterpret_cast<scene_rdl2::rdl2::EvalNormalFunc>(mIspc.mAttrFuncs.mEvalSubsurfaceNormal);
    }

    void * getCastsCausticsISPCFunc() const override
    {
        return ispc::DwaBase_getCastsCausticsFunc();
    }

    void * getResolveSubsurfaceTypeISPCFunc() const override
    {
        return ispc::DwaBase_getResolveSubsurfaceTypeFunc();
    }

    void * getResolveParametersISPCFunc() const override
    {
        return ispc::DwaBase_getResolveParametersFunc();
    }

    void * getResolvePresenceISPCFunc() const override
    {
        return ispc::DwaBase_getResolvePresenceFunc();
    }

    void * getResolvePreventLightCullingISPCFunc() const override
    {
        return ispc::DwaBase_getResolvePreventLightCullingFunc();
    }

    void * getResolveSubsurfaceNormalISPCFunc() const override
    {
        return ispc::DwaBase_getresolveSubsurfaceNormalFunc();
    }

    const ispc::DwaBase* getISPCBaseMaterialStruct() const { return &mIspc; }
    ispc::DwaBase* getISPCBaseMaterialStruct() { return &mIspc; }

    DwaBaseAttributeKeys* getAttributeKeys() { return &mAttrKeys; }

    finline const moonray::shading::Xform* xform() const
    {
        return mXform.get();
    }

    finline const glitter::Glitter* getGlitterPointer() const
    {
        return mGlitterPointer.get();
    }
    // ---------------------------

private:

    ispc::DwaBase           mIspc;
    DwaBaseAttributeKeys    mAttrKeys;
    std::unique_ptr<moonray::shading::Xform> mXform;
    std::unique_ptr<glitter::Glitter> mGlitterPointer;
};

} // namespace dwabase
} // namespace moonshine

