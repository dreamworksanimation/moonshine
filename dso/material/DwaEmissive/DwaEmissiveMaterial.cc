// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file DwaEmissiveMaterial.cc

#include "attributes.cc"
#include "labels.h"
#include "DwaEmissiveMaterial_ispc_stubs.h"

#include <moonshine/material/dwabase/DwaBase.h>
#include <moonshine/material/dwabase/DwaBaseLayerable.h>
#include <moonray/rendering/shading/MaterialApi.h>

#include <string>

using namespace moonray::shading;
using namespace moonshine::dwabase;

namespace {

DECLARE_DWA_BASE_LABELS()

DwaBaseAttributeKeys
collectAttributeKeys()
{
    DwaBaseAttributeKeys keys;
    keys.mShowEmission                  = attrShowEmission;
    keys.mEmission                      = attrEmission;
    keys.mPresence                      = attrPresence;

    return keys;
}

} // end anonymous namespace

RDL2_DSO_CLASS_BEGIN(DwaEmissiveMaterial, DwaBase)

public:
    DwaEmissiveMaterial(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name);
    ~DwaEmissiveMaterial() { }

private:
    void update() override;
    static void shade(const scene_rdl2::rdl2::Material* self, moonray::shading::TLState *tls,
                      const State& state, BsdfBuilder& bsdfBuilder);

RDL2_DSO_CLASS_END(DwaEmissiveMaterial)

DwaEmissiveMaterial::DwaEmissiveMaterial(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name) :
    DwaBase(sceneClass,
            name,
            collectAttributeKeys(),
            ispc::DwaEmissiveMaterial_collectAttributeFuncs(),
            sLabels,
            ispc::Model::Mixed)
{
    mType |= scene_rdl2::rdl2::INTERFACE_DWABASE;
    mType |= scene_rdl2::rdl2::INTERFACE_DWABASELAYERABLE;

    mShadeFunc = DwaEmissiveMaterial::shade;
    mShadeFuncv = (scene_rdl2::rdl2::ShadeFuncv) ispc::DwaEmissiveMaterial_getShadeFunc();
}



void
DwaEmissiveMaterial::update()
{
    // must call DwaBase::update()!
    DwaBase::update();
}

void
DwaEmissiveMaterial::shade(const scene_rdl2::rdl2::Material* self, moonray::shading::TLState *tls,
                           const State& state, BsdfBuilder& bsdfBuilder)
{
    const DwaEmissiveMaterial* me = static_cast<const DwaEmissiveMaterial*>(self);
    const ispc::DwaBase* dwabase = me->getISPCBaseMaterialStruct();

    ispc::DwaBaseParameters params;
    initColorCorrectParameters(params);

    me->resolveParameters(tls, state, false, params);
    me->createLobes(me, tls, state, bsdfBuilder, params, dwabase->mUParams, sLabels);
}

