// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file DwaMetalMaterial.cc

#include "attributes.cc"
#include "labels.h"
#include "DwaMetalMaterial_ispc_stubs.h"

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
    keys.mMetallicColor                 = attrMetallicColor;
    keys.mMetallicEdgeColor             = attrMetallicEdgeColor;
    keys.mRoughness                     = attrRoughness;
    keys.mAnisotropy                    = attrAnisotropy;
    keys.mShadingTangent                = attrShadingTangent;
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

RDL2_DSO_CLASS_BEGIN(DwaMetalMaterial, DwaBase)

public:
    DwaMetalMaterial(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name);
    ~DwaMetalMaterial() { }

private:
    void update() override;
    static void shade(const scene_rdl2::rdl2::Material* self, moonray::shading::TLState *tls,
                      const State& state, BsdfBuilder& bsdfBuilder);

RDL2_DSO_CLASS_END(DwaMetalMaterial)

DwaMetalMaterial::DwaMetalMaterial(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name) :
    DwaBase(sceneClass, name,
            collectAttributeKeys(),
            ispc::DwaMetalMaterial_collectAttributeFuncs() ,
            sLabels,
            ispc::Model::Metal)
{
    mType |= scene_rdl2::rdl2::INTERFACE_DWABASELAYERABLE;

    mShadeFunc = DwaMetalMaterial::shade;
    mShadeFuncv = (scene_rdl2::rdl2::ShadeFuncv) ispc::DwaMetalMaterial_getShadeFunc();
}

void
DwaMetalMaterial::update()
{
    // must call DwaBase::update()!
    DwaBase::update();
}

void
DwaMetalMaterial::shade(const scene_rdl2::rdl2::Material* self, moonray::shading::TLState *tls,
                        const State& state, BsdfBuilder& bsdfBuilder)
{
    const DwaMetalMaterial* me = static_cast<const DwaMetalMaterial*>(self);
    const ispc::DwaBase* dwabase = me->getISPCBaseMaterialStruct();

    ispc::DwaBaseParameters params;
    me->resolveParameters(tls, state, me->get(attrCastsCaustics), params);
    me->createLobes(me, tls, state, bsdfBuilder, params, dwabase->mUParams, sLabels);
}

