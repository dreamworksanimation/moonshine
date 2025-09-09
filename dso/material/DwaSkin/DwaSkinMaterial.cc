// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file DwaSkinMaterial.cc

#include "attributes.cc"
#include "labels.h"
#include "DwaSkinMaterial_ispc_stubs.h"

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
    // Skin labels outer_specular as "Moisture"
    keys.mShowOuterSpecular             = attrShowMoisture;
    keys.mOuterSpecular                 = attrMoistureMask;
    keys.mOuterSpecularModel            = attrMoistureModel;
    keys.mOuterSpecularRefractiveIndex  = attrMoistureRefractiveIndex;
    keys.mOuterSpecularRoughness        = attrMoistureRoughness;
    keys.mUseOuterSpecularNormal        = attrUseMoistureNormal;
    keys.mOuterSpecularNormal           = attrMoistureNormal;
    keys.mOuterSpecularNormalDial       = attrMoistureNormalDial;
    keys.mRoughness                     = attrRoughness;
    keys.mAnisotropy                    = attrAnisotropy;
    keys.mShadingTangent                = attrShadingTangent;
    keys.mShowDiffuse                   = attrShowDiffuse;
    keys.mAlbedo                        = attrAlbedo;
    keys.mDiffuseRoughness              = attrDiffuseRoughness;
    keys.mSubsurface                    = attrSubsurface;
    keys.mScatteringColor               = attrScatteringColor;
    keys.mScatteringRadius              = attrScatteringRadius;
    keys.mCreaseAttenuation             = attrCreaseAttenuation;
    keys.mSubsurfaceTraceSet            = attrSubsurfaceTraceSet;
    keys.mEnableSubsurfaceInputNormal   = attrEnableSubsurfaceInputNormal;
    keys.mSSSResolveSelfIntersections   = attrSSSResolveSelfIntersections;
    keys.mDiffuseTransmission           = attrDiffuseTransmission;
    keys.mDiffuseTransmissionColor      = attrDiffuseTransmissionColor;
    keys.mDiffuseTransmissionBlendingBehavior = attrDiffuseTransmissionBlendingBehavior;
    keys.mShowEmission                  = attrShowEmission;
    keys.mEmission                      = attrEmission;
    keys.mInputNormal                   = attrInputNormal;
    keys.mPresence                      = attrPresence;
    keys.mInputNormalDial               = attrInputNormalDial;
    keys.mNormalAAStrategy              = attrNormalAAStrategy;
    keys.mNormalAADial                  = attrNormalAADial;
    keys.mThinGeometry                  = attrThinGeometry;
    keys.mCastsCaustics                 = attrCastsCaustics;

    return keys;
}

} // end anonymous namespace

RDL2_DSO_CLASS_BEGIN(DwaSkinMaterial, DwaBase)

public:
    DwaSkinMaterial(const SceneClass& sceneClass, const std::string& name);
    ~DwaSkinMaterial() { }

private:
    void update() override;
    static void shade(const Material* self, moonray::shading::TLState *tls,
                      const State& state, BsdfBuilder& bsdfBuilder);

    static Vec3f evalSubsurfaceNormal(const Material* self,
                                      moonray::shading::TLState *tls,
                                      const moonray::shading::State& state);

RDL2_DSO_CLASS_END(DwaSkinMaterial)


DwaSkinMaterial::DwaSkinMaterial(const SceneClass& sceneClass, const std::string& name) :
    DwaBase(sceneClass,
            name,
            collectAttributeKeys(),
            ispc::DwaSkinMaterial_collectAttributeFuncs(),
            sLabels,
            ispc::Model::SolidDielectric)
{
    mType |= INTERFACE_DWABASE;
    mType |= INTERFACE_DWABASELAYERABLE;

    mShadeFunc = DwaSkinMaterial::shade;
    mShadeFuncv = (ShadeFuncv) ispc::DwaSkinMaterial_getShadeFunc();
}

void
DwaSkinMaterial::update()
{
    // must call DwaBase::update()!
    DwaBase::update();

    // set bssrdf normal map
    ispc::DwaBase* dwabase = getISPCBaseMaterialStruct();
    dwabase->mAttrFuncs.mEvalSubsurfaceNormal = getEnableSubsurfaceInputNormal() ?
            (intptr_t)DwaSkinMaterial::evalSubsurfaceNormal : 0;

    if (get(attrDiffuseLightSet)) {
        dwabase->mDiffuseLightSet = get(attrDiffuseLightSet)->asA<scene_rdl2::rdl2::LightSet>();
    }
    if (get(attrSpecularLightSet)) {
        dwabase->mSpecularLightSet = get(attrSpecularLightSet)->asA<scene_rdl2::rdl2::LightSet>();
    }
}

void
DwaSkinMaterial::shade(const Material* self, moonray::shading::TLState *tls,
                       const State& state, BsdfBuilder& bsdfBuilder)
{
    const DwaSkinMaterial* me = static_cast<const DwaSkinMaterial*>(self);
    const ispc::DwaBase* dwabase = me->getISPCBaseMaterialStruct();

    ispc::DwaBaseParameters params;
    initColorCorrectParameters(params);

    me->resolveParameters(tls, state, me->get(attrCastsCaustics), params);
    me->createLobes(me, tls, state, bsdfBuilder, params, dwabase->mUParams, sLabels);
}

Vec3f
DwaSkinMaterial::evalSubsurfaceNormal(const Material* self,
                                  moonray::shading::TLState *tls,
                                  const moonray::shading::State& state)
{
    const DwaSkinMaterial* me = static_cast<const DwaSkinMaterial*>(self);
    return me->resolveSubsurfaceNormal(tls, state);
}

