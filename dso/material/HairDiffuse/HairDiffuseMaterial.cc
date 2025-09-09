// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

///
/// @file HairDiffuseMaterial.cc
/// $Id$
///
//

#include "attributes.cc"
#include "HairDiffuseMaterial_ispc_stubs.h"
#include "labels.h"

#include <moonshine/material/dwabase/DwaBase.h>
#include <moonshine/material/dwabase/DwaBaseLayerable.h>
#include <moonray/rendering/shading/MaterialApi.h>

using namespace scene_rdl2::math;
using namespace moonray::shading;
using namespace moonshine::dwabase;

namespace {

DECLARE_DWA_BASE_LABELS()

DwaBaseAttributeKeys
collectAttributeKeys()
{
    DwaBaseAttributeKeys keys;

    keys.mCastsCaustics = attrCastsCaustics;
    keys.mPresence = attrPresence;
    keys.mShowEmission = attrShowEmission;
    keys.mEmission = attrEmission;

    keys.mHairColor = attrHairColor;
    keys.mHairiDiffuseUseIndependentFrontAndBackColor = attrUseIndependentFrontAndBackHairColor;
    keys.mHairDiffuseFrontColor = attrFrontHairColor;
    keys.mHairDiffuseBackColor = attrBackHairColor;

    keys.mHairSubsurfaceBlend = attrHairSubsurfaceBlend;
    keys.mSubsurface = attrHairSubsurface;
    keys.mScatteringColor = attrHairScatteringColor;
    keys.mScatteringRadius = attrHairScatteringRadius;
    keys.mSubsurfaceTraceSet = attrHairSubsurfaceTraceSet;
    keys.mEnableSubsurfaceInputNormal = attrHairEnableSubsurfaceInputNormal;

    keys.mInputNormal = attrHairInputNormal;
    keys.mInputNormalDial = attrHairInputNormalDial;

    return keys;
}

} // end anonymous namespace


//---------------------------------------------------------------------------
/// @brief A Kajiya Kay Diffuse Hair Shading Material
RDL2_DSO_CLASS_BEGIN(HairDiffuseMaterial, DwaBase)

public:
    HairDiffuseMaterial(const SceneClass& sceneClass, const std::string& name);

private:
    virtual void update() override;

    static void shade(const Material* self,
                      moonray::shading::TLState *tls,
                      const moonray::shading::State& state,
                      moonray::shading::BsdfBuilder& bsdfBuilder);

    static Vec3f evalSubsurfaceNormal(const Material* self,
                                      moonray::shading::TLState *tls,
                                      const moonray::shading::State& state);

RDL2_DSO_CLASS_END(HairDiffuseMaterial)


HairDiffuseMaterial::HairDiffuseMaterial(const SceneClass& sceneClass, const std::string& name) :
    DwaBase(sceneClass, name,
            collectAttributeKeys(),
            ispc::HairDiffuseMaterial_collectAttributeFuncs(),
            sLabels,
            ispc::Model::HairDiffuse)
{
    mType |= INTERFACE_DWABASEHAIRLAYERABLE;

    mShadeFunc = HairDiffuseMaterial::shade;
    mShadeFuncv = (ShadeFuncv) ispc::HairDiffuseMaterial_getShadeFunc();
}

void
HairDiffuseMaterial::update()
{
    // must call DwaBase::update()!
    DwaBase::update();

        // set bssrdf normal map
    ispc::DwaBase* dwabase = getISPCBaseMaterialStruct();
    dwabase->mAttrFuncs.mEvalSubsurfaceNormal = getEnableSubsurfaceInputNormal() ?
            (intptr_t)HairDiffuseMaterial::evalSubsurfaceNormal : 0;
}

void
HairDiffuseMaterial::shade(const Material* self,
                           moonray::shading::TLState *tls,
                           const moonray::shading::State &state,
                           moonray::shading::BsdfBuilder &bsdfBuilder)
{
    const HairDiffuseMaterial* me = static_cast<const HairDiffuseMaterial*>(self);
    const ispc::DwaBase* dwabase = me->getISPCBaseMaterialStruct();

    ispc::DwaBaseParameters params;
    initColorCorrectParameters(params);

    me->resolveParameters(tls, state, false, params);
    me->createLobes(me, tls, state, bsdfBuilder, params, dwabase->mUParams, sLabels);
}

Vec3f
HairDiffuseMaterial::evalSubsurfaceNormal(const Material* self,
                                          moonray::shading::TLState *tls,
                                          const moonray::shading::State& state)
{
    const HairDiffuseMaterial* me = static_cast<const HairDiffuseMaterial*>(self);
    return me->resolveSubsurfaceNormal(tls, state);
}

//---------------------------------------------------------------------------

