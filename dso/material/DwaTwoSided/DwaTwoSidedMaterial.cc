// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

///
/// @file DwaTwoSidedMaterial.cc
/// $Id$
///

#include "attributes.cc"
#include "DwaTwoSidedMaterial_ispc_stubs.h"
#include "labels.h"

#include <moonray/common/mcrt_macros/moonray_static_check.h>
#include <moonshine/material/dwabase/DwaBase.h>
#include <moonshine/material/dwabase/Blending.h>
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
    return keys;
}

} // end anonymous namespace

//---------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(DwaTwoSidedMaterial, DwaBase)

public:
    DwaTwoSidedMaterial(const SceneClass& sceneClass, const std::string& name);

    void resolveUniformParameters(ispc::DwaBaseUniformParameters &uParams) const override;

    virtual void update() override;

    bool getCastsCaustics() const override;

    int resolveSubsurfaceType(const State& state) const override;

    bool resolveParameters(TLState *tls,
                           const State& state,
                           bool castsCaustics,
                           ispc::DwaBaseParameters &params) const override;

    Vec3f resolveSubsurfaceNormal(TLState *tls,
                                        const State& state) const override;

    float resolvePresence(TLState *tls,
                          const State& state) const override;

    float resolveRefractiveIndex(TLState *tls,
                                 const State& state) const override;

    void * getCastsCausticsISPCFunc() const override
    {
        return ispc::DwaTwoSidedMaterial_getCastsCausticsFunc();
    }

    void * getResolveSubsurfaceTypeISPCFunc() const override
    {
        return ispc::DwaTwoSidedMaterial_getResolveSubsurfaceTypeFunc();
    }

    void * getResolveParametersISPCFunc() const override
    {
        return ispc::DwaTwoSidedMaterial_getResolveParametersFunc();
    }

    void * getResolvePresenceISPCFunc() const override
    {
        return ispc::DwaTwoSidedMaterial_getResolvePresenceFunc();
    }

    void * getResolveSubsurfaceNormalISPCFunc() const override
    {
        return ispc::DwaTwoSidedMaterial_getresolveSubsurfaceNormalFunc();
    }

    EvalNormalFunc getEvalSubsurfaceNormalFunc() const override
    {
        return reinterpret_cast<EvalNormalFunc>(DwaTwoSidedMaterial::evalSubsurfaceNormal);
    }

    const ispc::DwaTwoSidedMaterial* getISPCLayerMaterialStruct() const { return &mIspc; }

private:
    static void shade(const Material* self,
                      TLState *tls,
                      const State& state,
                      BsdfBuilder& bsdfBuilder);

    static float presence(const Material* self,
                          TLState *tls,
                          const State& state);

    static Vec3f evalSubsurfaceNormal(const Material* self,
                                      TLState *tls,
                                      const State& state);

    ispc::DwaTwoSidedMaterial  mIspc;
    const DwaBaseLayerable* mFrontMaterial;
    const DwaBaseLayerable* mBackMaterial;

RDL2_DSO_CLASS_END(DwaTwoSidedMaterial)


//---------------------------------------------------------------------------

DwaTwoSidedMaterial::DwaTwoSidedMaterial(const SceneClass& sceneClass, const std::string& name)
    : DwaBase(sceneClass, name,
              collectAttributeKeys(),
              ispc::DwaTwoSidedMaterial_collectAttributeFuncs() ,
              sLabels,
              ispc::Model::Layer)
    , mFrontMaterial(nullptr)
    , mBackMaterial(nullptr)
{
    mType |= INTERFACE_DWABASE;
    mType |= INTERFACE_DWABASELAYERABLE;

    mShadeFunc = DwaTwoSidedMaterial::shade;
    mShadeFuncv = (ShadeFuncv) ispc::DwaTwoSidedMaterial_getShadeFunc();
    mPresenceFunc = DwaTwoSidedMaterial::presence;

    mIspc.mEvalSubsurfaceNormal = (intptr_t)DwaTwoSidedMaterial::evalSubsurfaceNormal;
}

void
DwaTwoSidedMaterial::resolveUniformParameters(ispc::DwaBaseUniformParameters &uParams) const
{
    ispc::DwaBaseUniformParameters uParamsFront, uParamsBack;
    mFrontMaterial->resolveUniformParameters(uParamsFront);
    mBackMaterial->resolveUniformParameters(uParamsBack);
    uParams = uParamsFront;
    blendUniformParameters(uParamsFront,
                           uParamsBack,
                           uParams,
                           get(attrFallbackSpecularModel),
                           get(attrFallbackOuterSpecularModel),
                           get(attrFallbackOuterSpecularUseBending),
                           get(attrFallbackBSSRDF),
                           true,    // attrFallbackThinGeometry
                           get(attrFallbackPreventLightCulling));


    // this geometry must be 'thin', why else use DwaTwoSidedMaterial ?
    uParams.mThinGeometry = true;
}

void
DwaTwoSidedMaterial::update()
{
    if (hasChanged(attrFrontMaterial) || hasChanged(attrBackMaterial)) {
        mFrontMaterial = registerLayerable(get(attrFrontMaterial), mIspc.mFrontMaterial);
        mBackMaterial = registerLayerable(get(attrBackMaterial), mIspc.mBackMaterial);
        resolveUniformParameters(mIspc.mUParams);
    }
    mIspc.mSubsurfaceTraceSet = (TraceSet *)get(attrSubsurfaceTraceSet);
}

bool
DwaTwoSidedMaterial::getCastsCaustics() const
{
    // This behavior matches the vector version's behavior, which is required
    // for deterministic results. See note in DwaTwoSidedMaterial.ispc DWATWOSIDED_castsCaustics()
    if (!mBackMaterial || !mFrontMaterial) {
        return false;
    } else {
        return mFrontMaterial->getCastsCaustics() || mBackMaterial->getCastsCaustics();
    }
}

int
DwaTwoSidedMaterial::resolveSubsurfaceType(const State& state) const
{
    return mIspc.mUParams.mSubsurface;
}

bool
DwaTwoSidedMaterial::resolveParameters(TLState *tls,
                                       const State& state,
                                       const bool castsCaustics,
                                       ispc::DwaBaseParameters &params) const
{
    bool result = false;

    if (state.isEntering() && mFrontMaterial != nullptr) {
        result = mFrontMaterial->resolveParameters(tls, state, castsCaustics, params);
    } else if (!state.isEntering() && mBackMaterial != nullptr) {
        result = mBackMaterial->resolveParameters(tls, state, castsCaustics, params);
    }

    return result;
}

float
DwaTwoSidedMaterial::resolvePresence(TLState *tls,
                                     const State& state) const
{
    float result = 1.f;

    if (state.isEntering() && mFrontMaterial != nullptr) {
        result = mFrontMaterial->resolvePresence(tls, state);
    } else if (!state.isEntering() && mBackMaterial != nullptr) {
        result = mBackMaterial->resolvePresence(tls, state);
    }

    return result;
}

float
DwaTwoSidedMaterial::resolveRefractiveIndex(TLState *tls,
                                            const State& state) const
{
    float result = 1.f;

    if (state.isEntering() && mFrontMaterial != nullptr) {
        result = mFrontMaterial->resolveRefractiveIndex(tls, state);
    } else if (!state.isEntering() && mBackMaterial != nullptr) {
        result = mBackMaterial->resolveRefractiveIndex(tls, state);
    }

    return result;
}

Vec3f
DwaTwoSidedMaterial::resolveSubsurfaceNormal(TLState *tls,
                                             const State& state) const

{
    Vec3f result(state.getN());

    if (state.isEntering() && mFrontMaterial != nullptr) {
        result = mFrontMaterial->resolveSubsurfaceNormal(tls, state);
    } else if (!state.isEntering() && mBackMaterial != nullptr) {
        result = mBackMaterial->resolveSubsurfaceNormal(tls, state);
    }

    return result;
}


void
DwaTwoSidedMaterial::shade(const Material* self,
                           TLState *tls,
                           const State& state,
                           BsdfBuilder& bsdfBuilder)
{
    const DwaTwoSidedMaterial* me = static_cast<const DwaTwoSidedMaterial*>(self);
    const ispc::DwaTwoSidedMaterial* ispc = me->getISPCLayerMaterialStruct();

    const bool castsCaustics = me->getCastsCaustics();

    ispc::DwaBaseParameters params;
    initColorCorrectParameters(params);

    if (me->resolveParameters(tls, state, castsCaustics, params)) {
        // get sss trace set for this DwaTwoSidedMaterial
        params.mSubsurfaceTraceSet = (TraceSet *)me->get(attrSubsurfaceTraceSet);

        me->createLobes(me, tls, state, bsdfBuilder, params, ispc->mUParams, sLabels);
    }
}

float
DwaTwoSidedMaterial::presence(const Material* self,
                              TLState *tls,
                              const State& state)
{
    const DwaTwoSidedMaterial* me = static_cast<const DwaTwoSidedMaterial*>(self);
    return me->resolvePresence(tls, state);
}

Vec3f
DwaTwoSidedMaterial::evalSubsurfaceNormal(const Material* self,
                                          TLState *tls,
                                          const State& state)
{
    const DwaTwoSidedMaterial* me = static_cast<const DwaTwoSidedMaterial*>(self);
    return me->resolveSubsurfaceNormal(tls, state);
}



// This is a construct to access the ISPC Data Struct from Inside the Scalar Material object.
// Note we are not using MATERIAL_GET_ISPC_CPTR to retrieve the ispc structure from the Material
// class as in other materials since that macro assumes a certain data layout we do not adhere to in
// DwaBase materials.
extern "C" {
const void*
getDwaTwoSidedMaterialStruct(const void* ptr)
{
    const DwaTwoSidedMaterial* classPtr = static_cast<const DwaTwoSidedMaterial*>(ptr);
    return (static_cast<const void*>(classPtr->getISPCLayerMaterialStruct()));
}
}

//---------------------------------------------------------------------------


