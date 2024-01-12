// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file DwaVelvetMaterial_v2.cc

#include "attributes.cc"
#include "labels.h"
#include "DwaVelvetMaterial_v2_ispc_stubs.h"

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

RDL2_DSO_CLASS_BEGIN(DwaVelvetMaterial_v2, DwaBase)

public:
    DwaVelvetMaterial_v2(const scene_rdl2::rdl2::SceneClass& sceneClass,
                             const std::string& name);
    ~DwaVelvetMaterial_v2() {}

private:
    virtual void update() override;
    static void shade(const scene_rdl2::rdl2::Material* self,
                      moonray::shading::TLState *tls,
                      const State& state,
                      BsdfBuilder& bsdfBuilder);

RDL2_DSO_CLASS_END(DwaVelvetMaterial_v2)

DwaVelvetMaterial_v2::DwaVelvetMaterial_v2(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name) :
        DwaBase(sceneClass, name,
                collectAttributeKeys(),
                ispc::DwaVelvetMaterial_v2_collectAttributeFuncs() ,
                sLabels,
                ispc::Model::Velvet_v2)
{
    mType |= scene_rdl2::rdl2::INTERFACE_DWABASELAYERABLE;

    mShadeFunc = DwaVelvetMaterial_v2::shade;
    mShadeFuncv = (scene_rdl2::rdl2::ShadeFuncv) ispc::DwaVelvetMaterial_v2_getShadeFunc();
}

void
DwaVelvetMaterial_v2::update()
{
    // must call DwaBase::update()!
    DwaBase::update();
}

void
DwaVelvetMaterial_v2::shade(const scene_rdl2::rdl2::Material* self, moonray::shading::TLState *tls,
                         const State& state, BsdfBuilder& bsdfBuilder)
{
    const DwaVelvetMaterial_v2* me = static_cast<const DwaVelvetMaterial_v2*>(self);
    const ispc::DwaBase* dwabase = me->getISPCBaseMaterialStruct();

    ispc::DwaBaseParameters params;
    me->resolveParameters(tls, state, me->get(attrCastsCaustics), params);
    me->createLobes(me, tls, state, bsdfBuilder, params, dwabase->mUParams, sLabels);
}


//---------------------------------------------------------------------------

