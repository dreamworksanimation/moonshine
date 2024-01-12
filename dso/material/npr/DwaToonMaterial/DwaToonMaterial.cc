// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file DwaToonMaterial.cc

#include "attributes.cc"
#include "labels.h"

#include "DwaToonMaterial_ispc_stubs.h"

#include <moonshine/material/dwabase/DwaBase.h>
#include <moonshine/material/dwabase/DwaBaseLayerable.h>
#include <moonray/rendering/shading/MaterialApi.h>

#include <string>

using namespace scene_rdl2::math;
using namespace moonray::shading;
using namespace moonshine::dwabase;

namespace {

DECLARE_DWA_BASE_LABELS()

DwaBaseAttributeKeys
collectAttributeKeys()
{
    DwaBaseAttributeKeys keys;
    ASSIGN_GLITTER_ATTR_KEYS(keys);
    ASSIGN_IRIDESCENCE_ATTR_KEYS(keys);
    ASSIGN_TOON_DIFFUSE_ATTR_KEYS(keys);

    keys.mToonSpecularKeys.mShow        = attrShowSpecular;
    keys.mToonSpecularKeys.mModel       = attrSpecularModel;
    keys.mToonSpecularKeys.mRoughness   = attrRoughness;
    ASSIGN_TOON_SPEC_ATTR_KEYS(keys, ToonSpecular);

    keys.mShowFuzz                      = attrShowFuzz;
    keys.mFuzz                          = attrFuzz;
    keys.mFuzzAlbedo                    = attrFuzzAlbedo;
    keys.mFuzzRoughness                 = attrFuzzRoughness;
    keys.mFuzzUseAbsorbingFibers        = attrFuzzUseAbsorbingFibers;
    keys.mFuzzNormal                    = attrFuzzNormal;
    keys.mFuzzNormalDial                = attrFuzzNormalDial;

    keys.mShowSpecular                  = attrShowSpecular;
    keys.mSpecular                      = attrSpecular;
    keys.mSpecularModel                 = attrSpecularModel;
    keys.mRefractiveIndex               = attrRefractiveIndex;
    keys.mMetallic                      = attrMetallic;
    keys.mMetallicColor                 = attrMetallicColor;
    keys.mMetallicEdgeColor             = attrMetallicEdgeColor;
    keys.mRoughness                     = attrRoughness;
    keys.mAnisotropy                    = attrAnisotropy;
    keys.mShadingTangent                = attrShadingTangent;
    keys.mShowTransmission              = attrShowTransmission;
    keys.mTransmission                  = attrTransmission;
    keys.mTransmissionColor             = attrTransmissionColor;
    keys.mUseIndependentTransmissionRefractiveIndex= attrUseIndependentTransmissionRefractiveIndex;
    keys.mIndependentTransmissionRefractiveIndex   = attrIndependentTransmissionRefractiveIndex;
    keys.mUseIndependentTransmissionRoughness      = attrUseIndependentTransmissionRoughness;
    keys.mIndependentTransmissionRoughness         = attrIndependentTransmissionRoughness;
    keys.mUseDispersion                 = attrUseDispersion;
    keys.mDispersionAbbeNumber          = attrDispersionAbbeNumber;
    keys.mShowDiffuse                   = attrShowDiffuse;
    keys.mAlbedo                        = attrAlbedo;
    keys.mDiffuseRoughness              = attrDiffuseRoughness;
    keys.mSubsurface                    = attrSubsurface;
    keys.mScatteringColor               = attrScatteringColor;
    keys.mScatteringRadius              = attrScatteringRadius;
    keys.mSubsurfaceTraceSet            = attrSubsurfaceTraceSet;
    keys.mEnableSubsurfaceInputNormal   = attrEnableSubsurfaceInputNormal;
    keys.mSSSResolveSelfIntersections   = attrSSSResolveSelfIntersections;
    keys.mDiffuseTransmission           = attrDiffuseTransmission;
    keys.mDiffuseTransmissionColor      = attrDiffuseTransmissionColor;
    keys.mDiffuseTransmissionBlendingBehavior = attrDiffuseTransmissionBlendingBehavior;
    keys.mShowOuterSpecular             = attrShowClearcoat;
    keys.mOuterSpecular                 = attrClearcoat;
    keys.mOuterSpecularModel            = attrClearcoatModel;
    keys.mOuterSpecularRefractiveIndex  = attrClearcoatRefractiveIndex;
    keys.mOuterSpecularRoughness        = attrClearcoatRoughness;
    keys.mOuterSpecularThickness        = attrClearcoatThickness;
    keys.mOuterSpecularAttenuationColor = attrClearcoatAttenuationColor;
    keys.mOuterSpecularUseBending       = attrClearcoatUseBending;
    keys.mUseOuterSpecularNormal        = attrUseClearcoatNormal;
    keys.mOuterSpecularNormal           = attrClearcoatNormal;
    keys.mOuterSpecularNormalDial       = attrClearcoatNormalDial;
    keys.mShowEmission                  = attrShowEmission;
    keys.mEmission                      = attrEmission;
    keys.mPresence                      = attrPresence;
    keys.mInputNormal                   = attrInputNormal;
    keys.mInputNormalDial               = attrInputNormalDial;
    keys.mNormalAAStrategy              = attrNormalAAStrategy;
    keys.mNormalAADial                  = attrNormalAADial;
    keys.mThinGeometry                  = attrThinGeometry;
    keys.mCastsCaustics                 = attrCastsCaustics;
    keys.mPreventLightCulling           = attrPreventLightCulling;

    return keys;
}

} // end anonymous namespace

RDL2_DSO_CLASS_BEGIN(DwaToonMaterial, DwaBase)

public:
    DwaToonMaterial(const SceneClass& sceneClass, const std::string& name);
    ~DwaToonMaterial() { }

private:
    void update() override;
    static void shade(const Material* self, moonray::shading::TLState *tls,
                      const State& state, BsdfBuilder& bsdfBuilder);

    static Vec3f evalSubsurfaceNormal(const Material* self,
                                      moonray::shading::TLState *tls,
                                      const moonray::shading::State& state);

RDL2_DSO_CLASS_END(DwaToonMaterial)

DwaToonMaterial::DwaToonMaterial(const SceneClass& sceneClass, const std::string& name) :
    DwaBase(sceneClass,
            name,
            collectAttributeKeys(),
            ispc::DwaToonMaterial_collectAttributeFuncs(),
            sLabels,
            ispc::Model::Toon)
{
    mType |= INTERFACE_DWABASELAYERABLE;

    mShadeFunc = DwaToonMaterial::shade;
    mShadeFuncv = (ShadeFuncv) ispc::DwaToonMaterial_getShadeFunc();
}

void
DwaToonMaterial::update()
{
    // must call DwaBase::update()!
    DwaBase::update();

    // set bssrdf normal map
    ispc::DwaBase* dwabase = getISPCBaseMaterialStruct();
    dwabase->mAttrFuncs.mEvalSubsurfaceNormal = getEnableSubsurfaceInputNormal() ?
            (intptr_t)DwaToonMaterial::evalSubsurfaceNormal : 0;

    const std::vector<float>& points  = get(attrToonDRampPositions);
    const std::vector<Color>& colors  = get(attrToonDRampColors);
    const std::vector<int>&   interps = get(attrToonDRampInterpolators);
    dwabase->mToonDiffuseData.mRampNumPoints = points.size();
    for (size_t i = 0; i < points.size(); ++i) {
        dwabase->mToonDiffuseData.mRampPositions[i]     = points[i];
        asCpp(dwabase->mToonDiffuseData.mRampColors[i]) = colors[i];
        dwabase->mToonDiffuseData.mRampInterpolators[i] = (ispc::RampInterpolatorMode)interps[i];
    }

    DwaBaseAttributeKeys *keys = getAttributeKeys();
    keys->mToonDiffuseKeys.mRampColors.push_back(attrToonDRampColorMultiplier0);
    keys->mToonDiffuseKeys.mRampColors.push_back(attrToonDRampColorMultiplier1);
    keys->mToonDiffuseKeys.mRampColors.push_back(attrToonDRampColorMultiplier2);
    keys->mToonDiffuseKeys.mRampColors.push_back(attrToonDRampColorMultiplier3);
    keys->mToonDiffuseKeys.mRampColors.push_back(attrToonDRampColorMultiplier4);
    keys->mToonDiffuseKeys.mRampColors.push_back(attrToonDRampColorMultiplier5);
    keys->mToonDiffuseKeys.mRampColors.push_back(attrToonDRampColorMultiplier6);
    keys->mToonDiffuseKeys.mRampColors.push_back(attrToonDRampColorMultiplier7);
    keys->mToonDiffuseKeys.mRampColors.push_back(attrToonDRampColorMultiplier8);
    keys->mToonDiffuseKeys.mRampColors.push_back(attrToonDRampColorMultiplier9);

    keys->mToonDiffuseKeys.mRampOffsets.push_back(attrToonDRampPositionOffset0);
    keys->mToonDiffuseKeys.mRampOffsets.push_back(attrToonDRampPositionOffset1);
    keys->mToonDiffuseKeys.mRampOffsets.push_back(attrToonDRampPositionOffset2);
    keys->mToonDiffuseKeys.mRampOffsets.push_back(attrToonDRampPositionOffset3);
    keys->mToonDiffuseKeys.mRampOffsets.push_back(attrToonDRampPositionOffset4);
    keys->mToonDiffuseKeys.mRampOffsets.push_back(attrToonDRampPositionOffset5);
    keys->mToonDiffuseKeys.mRampOffsets.push_back(attrToonDRampPositionOffset6);
    keys->mToonDiffuseKeys.mRampOffsets.push_back(attrToonDRampPositionOffset7);
    keys->mToonDiffuseKeys.mRampOffsets.push_back(attrToonDRampPositionOffset8);
    keys->mToonDiffuseKeys.mRampOffsets.push_back(attrToonDRampPositionOffset9);

    const std::vector<float>& specPoints  = get(attrToonSpecularRampPositions);
    const std::vector<float>& specValues  = get(attrToonSpecularRampValues);
    const std::vector<int>&   specInterps = get(attrToonSpecularRampInterpolators);
    dwabase->mToonSpecularData.mRampNumPoints = specPoints.size();
    for (size_t i = 0; i < specPoints.size(); ++i) {
        dwabase->mToonSpecularData.mRampPositions[i] = specPoints[i];
        dwabase->mToonSpecularData.mRampValues[i] = specValues[i];
        dwabase->mToonSpecularData.mRampInterpolators[i] = (ispc::RampInterpolatorMode)specInterps[i];
    }

    keys->mToonSpecularKeys.mRampMultipliers.push_back(attrToonSpecularRampMultiplier0);
    keys->mToonSpecularKeys.mRampMultipliers.push_back(attrToonSpecularRampMultiplier1);
    keys->mToonSpecularKeys.mRampMultipliers.push_back(attrToonSpecularRampMultiplier2);
    keys->mToonSpecularKeys.mRampMultipliers.push_back(attrToonSpecularRampMultiplier3);
    keys->mToonSpecularKeys.mRampMultipliers.push_back(attrToonSpecularRampMultiplier4);
    keys->mToonSpecularKeys.mRampMultipliers.push_back(attrToonSpecularRampMultiplier5);
    keys->mToonSpecularKeys.mRampMultipliers.push_back(attrToonSpecularRampMultiplier6);
    keys->mToonSpecularKeys.mRampMultipliers.push_back(attrToonSpecularRampMultiplier7);
    keys->mToonSpecularKeys.mRampMultipliers.push_back(attrToonSpecularRampMultiplier8);
    keys->mToonSpecularKeys.mRampMultipliers.push_back(attrToonSpecularRampMultiplier9);

    keys->mToonSpecularKeys.mRampOffsets.push_back(attrToonSpecularRampPositionOffset0);
    keys->mToonSpecularKeys.mRampOffsets.push_back(attrToonSpecularRampPositionOffset1);
    keys->mToonSpecularKeys.mRampOffsets.push_back(attrToonSpecularRampPositionOffset2);
    keys->mToonSpecularKeys.mRampOffsets.push_back(attrToonSpecularRampPositionOffset3);
    keys->mToonSpecularKeys.mRampOffsets.push_back(attrToonSpecularRampPositionOffset4);
    keys->mToonSpecularKeys.mRampOffsets.push_back(attrToonSpecularRampPositionOffset5);
    keys->mToonSpecularKeys.mRampOffsets.push_back(attrToonSpecularRampPositionOffset6);
    keys->mToonSpecularKeys.mRampOffsets.push_back(attrToonSpecularRampPositionOffset7);
    keys->mToonSpecularKeys.mRampOffsets.push_back(attrToonSpecularRampPositionOffset8);
    keys->mToonSpecularKeys.mRampOffsets.push_back(attrToonSpecularRampPositionOffset9);
}

void
DwaToonMaterial::shade(const Material* self,
                       moonray::shading::TLState *tls,
                       const State& state,
                       BsdfBuilder& bsdfBuilder)
{
    const DwaToonMaterial* me = static_cast<const DwaToonMaterial*>(self);
    const ispc::DwaBase* dwabase = me->getISPCBaseMaterialStruct();

    ispc::DwaBaseParameters params;
    me->resolveParameters(tls, state, me->get(attrCastsCaustics), params);
    me->createLobes(me, tls, state, bsdfBuilder, params, dwabase->mUParams, sLabels);
}

Vec3f
DwaToonMaterial::evalSubsurfaceNormal(const Material* self,
                                      moonray::shading::TLState *tls,
                                      const moonray::shading::State& state)
{
    const DwaToonMaterial* me = static_cast<const DwaToonMaterial*>(self);
    return me->resolveSubsurfaceNormal(tls, state);
}

