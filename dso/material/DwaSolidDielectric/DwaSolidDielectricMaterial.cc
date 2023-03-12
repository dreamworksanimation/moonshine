// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file DwaSolidDielectricMaterial.cc

#include "attributes.cc"
#include "labels.h"

#include "DwaSolidDielectricMaterial_ispc_stubs.h"

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
    keys.mRoughness                     = attrRoughness;
    keys.mAnisotropy                    = attrAnisotropy;
    keys.mShadingTangent                = attrShadingTangent;
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

    return keys;
}

} // end anonymous namespace

RDL2_DSO_CLASS_BEGIN(DwaSolidDielectricMaterial, DwaBase)

public:
    DwaSolidDielectricMaterial(const SceneClass& sceneClass, const std::string& name);
    ~DwaSolidDielectricMaterial() { }

private:
    void update() override;
    static void shade(const Material* self, moonray::shading::TLState *tls,
                      const State& state, BsdfBuilder& bsdfBuilder);

    static Vec3f evalSubsurfaceNormal(const Material* self,
                                      moonray::shading::TLState *tls,
                                      const moonray::shading::State& state);


RDL2_DSO_CLASS_END(DwaSolidDielectricMaterial)


DwaSolidDielectricMaterial::DwaSolidDielectricMaterial(const SceneClass& sceneClass, const std::string& name) :
    DwaBase(sceneClass,
            name,
            collectAttributeKeys(),
            ispc::DwaSolidDielectricMaterial_collectAttributeFuncs(),
            sLabels,
            ispc::Model::SolidDielectric)
{
    mType |= INTERFACE_DWABASELAYERABLE;

    mShadeFunc = DwaSolidDielectricMaterial::shade;
    mShadeFuncv = (ShadeFuncv) ispc::DwaSolidDielectricMaterial_getShadeFunc();
}

void
DwaSolidDielectricMaterial::update()
{
    // must call DwaBase::update()!
    DwaBase::update();

    // set bssrdf normal map
    ispc::DwaBase* dwabase = getISPCBaseMaterialStruct();
    dwabase->mAttrFuncs.mEvalSubsurfaceNormal = getEnableSubsurfaceInputNormal() ?
            (intptr_t)DwaSolidDielectricMaterial::evalSubsurfaceNormal : 0;
}

void
DwaSolidDielectricMaterial::shade(const Material* self, moonray::shading::TLState *tls,
                       const State& state, BsdfBuilder& bsdfBuilder)
{
    const DwaSolidDielectricMaterial* me = static_cast<const DwaSolidDielectricMaterial*>(self);
    const ispc::DwaBase* dwabase = me->getISPCBaseMaterialStruct();

    ispc::DwaBaseParameters params;
    me->resolveParameters(tls, state, me->get(attrCastsCaustics), params);
    me->createLobes(me, tls, state, bsdfBuilder, params, dwabase->mUParams, sLabels);
}

Vec3f
DwaSolidDielectricMaterial::evalSubsurfaceNormal(const Material* self,
                                                 moonray::shading::TLState *tls,
                                                 const moonray::shading::State& state)
{
    const DwaSolidDielectricMaterial* me = static_cast<const DwaSolidDielectricMaterial*>(self);
    return me->resolveSubsurfaceNormal(tls, state);
}

