// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file DwaFabricMaterial.cc

#include "attributes.cc"
#include "labels.h"
#include "DwaFabricMaterial_ispc_stubs.h"

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

    keys.mShowFuzz                      = attrShowFuzz;
    keys.mFuzz                          = attrFuzz;
    keys.mFuzzAlbedo                    = attrFuzzAlbedo;
    keys.mFuzzRoughness                 = attrFuzzRoughness;
    keys.mFuzzUseAbsorbingFibers        = attrFuzzUseAbsorbingFibers;
    keys.mFuzzNormal                    = attrFuzzNormal;
    keys.mFuzzNormalDial                = attrFuzzNormalDial;

    keys.mShowDiffuse                   = attrShowDiffuse;
    keys.mAlbedo                        = attrAlbedo;
    keys.mDiffuseRoughness              = attrDiffuseRoughness;
    keys.mDiffuseTransmission           = attrDiffuseTransmission;
    keys.mDiffuseTransmissionColor      = attrDiffuseTransmissionColor;
    keys.mDiffuseTransmissionBlendingBehavior = attrDiffuseTransmissionBlendingBehavior;
    keys.mShowFabricSpecular            = attrShowFabricSpecular;
    keys.mWarpColor                     = attrWarpColor;
    keys.mWarpRoughness                 = attrWarpRoughness;
    keys.mUseIndependentWeftAttributes  = attrUseIndependentWeftAttributes;
    keys.mWeftRoughness                 = attrWeftRoughness;
    keys.mWeftColor                     = attrWeftColor;
    keys.mUseUVsForThreadDirection      = attrUseUVsForThreadDirection;
    keys.mWarpThreadDirection           = attrWarpThreadDirection;
    keys.mWarpThreadCoverage            = attrWarpThreadCoverage;
    keys.mWarpThreadElevation           = attrWarpThreadElevation;
    keys.mFabricDiffuseScattering       = attrFabricDiffuseScattering;
    keys.mShowEmission                  = attrShowEmission;
    keys.mEmission                      = attrEmission;
    keys.mPresence                      = attrPresence;
    keys.mInputNormal                   = attrInputNormal;
    keys.mInputNormalDial               = attrInputNormalDial;
    keys.mThinGeometry                  = attrThinGeometry;
    keys.mCastsCaustics                 = attrCastsCaustics;

    return keys;
}

} // end anonymous namespace
//---------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(DwaFabricMaterial, DwaBase)

public:
    DwaFabricMaterial(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name);
    ~DwaFabricMaterial() {}

private:
    virtual void update() override;

    static void shade(const scene_rdl2::rdl2::Material* self, moonray::shading::TLState *tls,
                      const State& state, BsdfBuilder& bsdfBuilder);

RDL2_DSO_CLASS_END(DwaFabricMaterial)

DwaFabricMaterial::DwaFabricMaterial(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name) :
        DwaBase(sceneClass, name,
                collectAttributeKeys(),
                ispc::DwaFabricMaterial_collectAttributeFuncs() ,
                sLabels,
                ispc::Model::Fabric)
{
    mType |= scene_rdl2::rdl2::INTERFACE_DWABASE;
    mType |= scene_rdl2::rdl2::INTERFACE_DWABASELAYERABLE;

    mShadeFunc = DwaFabricMaterial::shade;
    mShadeFuncv = (scene_rdl2::rdl2::ShadeFuncv) ispc::DwaFabricMaterial_getShadeFunc();
}

void
DwaFabricMaterial::update()
{
    // must call DwaBase::update()!
    DwaBase::update();
}

void
DwaFabricMaterial::shade(const scene_rdl2::rdl2::Material* self, moonray::shading::TLState *tls,
                         const State& state, BsdfBuilder& bsdfBuilder)
{
    const DwaFabricMaterial* me = static_cast<const DwaFabricMaterial*>(self);
    const ispc::DwaBase* dwabase = me->getISPCBaseMaterialStruct();

    ispc::DwaBaseParameters params;
    initColorCorrectParameters(params);

    me->resolveParameters(tls, state, me->get(attrCastsCaustics), params);
    me->createLobes(me, tls, state, bsdfBuilder, params, dwabase->mUParams, sLabels);
}


//---------------------------------------------------------------------------

