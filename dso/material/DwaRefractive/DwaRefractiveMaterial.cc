// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file DwaRefractiveMaterial.cc

#include "attributes.cc"
#include "labels.h"

#include "DwaRefractiveMaterial_ispc_stubs.h"

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
    keys.mShowTransmission              = attrShowTransmission;
    keys.mTransmissionColor             = attrTransmissionColor;
    keys.mUseIndependentTransmissionRefractiveIndex= attrUseIndependentTransmissionRefractiveIndex;
    keys.mIndependentTransmissionRefractiveIndex   = attrIndependentTransmissionRefractiveIndex;
    keys.mUseIndependentTransmissionRoughness      = attrUseIndependentTransmissionRoughness;
    keys.mIndependentTransmissionRoughness         = attrIndependentTransmissionRoughness;
    keys.mUseDispersion                     = attrUseDispersion;
    keys.mDispersionAbbeNumber              = attrDispersionAbbeNumber;
    keys.mShowOuterSpecular                 = attrShowClearcoat;
    keys.mOuterSpecular                     = attrClearcoat;
    keys.mOuterSpecularModel                = attrClearcoatModel;
    keys.mOuterSpecularRefractiveIndex      = attrClearcoatRefractiveIndex;
    keys.mOuterSpecularRoughness            = attrClearcoatRoughness;
    keys.mOuterSpecularThickness            = attrClearcoatThickness;
    keys.mOuterSpecularAttenuationColor     = attrClearcoatAttenuationColor;
    keys.mOuterSpecularUseBending           = attrClearcoatUseBending;
    keys.mUseOuterSpecularNormal            = attrUseClearcoatNormal;
    keys.mOuterSpecularNormal               = attrClearcoatNormal;
    keys.mOuterSpecularNormalDial           = attrClearcoatNormalDial;
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

RDL2_DSO_CLASS_BEGIN(DwaRefractiveMaterial, DwaBase)

public:
    DwaRefractiveMaterial(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name);
    ~DwaRefractiveMaterial() { }

private:
    void update() override;
    static void shade(const scene_rdl2::rdl2::Material* self, moonray::shading::TLState *tls,
                      const State& state, BsdfBuilder& bsdfBuilder);

RDL2_DSO_CLASS_END(DwaRefractiveMaterial)

DwaRefractiveMaterial::DwaRefractiveMaterial(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name) :
    DwaBase(sceneClass,
            name,
            collectAttributeKeys(),
            ispc::DwaRefractiveMaterial_collectAttributeFuncs(),
            sLabels,
            ispc::Model::Refractive)
{
    mType |= scene_rdl2::rdl2::INTERFACE_DWABASELAYERABLE;

    mShadeFunc = DwaRefractiveMaterial::shade;
    mShadeFuncv = (scene_rdl2::rdl2::ShadeFuncv) ispc::DwaRefractiveMaterial_getShadeFunc();
}



void
DwaRefractiveMaterial::update()
{
    // must call DwaBase::update()!
    DwaBase::update();
}

void
DwaRefractiveMaterial::shade(const scene_rdl2::rdl2::Material* self, moonray::shading::TLState *tls,
                       const State& state, BsdfBuilder& bsdfBuilder)
{
    const DwaRefractiveMaterial* me = static_cast<const DwaRefractiveMaterial*>(self);
    const ispc::DwaBase* dwabase = me->getISPCBaseMaterialStruct();

    ispc::DwaBaseParameters params;
    me->resolveParameters(tls, state, me->get(attrCastsCaustics), params);
    me->createLobes(me, tls, state, bsdfBuilder, params, dwabase->mUParams, sLabels);
}

