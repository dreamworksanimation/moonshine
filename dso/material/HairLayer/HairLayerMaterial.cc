// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

///
/// @file HairLayerMaterial.cc
/// $Id$
///

#include "attributes.cc"
#include "HairLayerMaterial_ispc_stubs.h"
#include "labels.h"

#include <moonray/common/mcrt_macros/moonray_static_check.h>
#include <moonray/rendering/shading/MaterialApi.h>

#include <moonshine/material/dwabase/DwaBase.h>
#include <moonshine/material/dwabase/DwaBaseLayerable.h>
#include <moonshine/material/dwabase/Blending.h>

#include <string>

using namespace scene_rdl2::math;
using namespace moonray::shading;
using namespace moonshine::dwabase;

static ispc::HairLayerMaterialStaticData sStaticHairLayerMaterialData;

namespace {

DECLARE_DWA_BASE_LABELS()

} // end anonymous namespace

//---------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(HairLayerMaterial, DwaBaseLayerable)

public:
    HairLayerMaterial(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name);

    void resolveUniformParameters(ispc::DwaBaseUniformParameters &uParams) const override;

    virtual void update() override;

    bool getCastsCaustics() const override;

    int resolveSubsurfaceType(const State& state) const override;

    bool resolveParameters(TLState *tls,
                           const State& state,
                           bool castsCaustics,
                           ispc::DwaBaseParameters &params) const override;

    scene_rdl2::math::Vec3f resolveSubsurfaceNormal(TLState *tls,
                                        const State& state) const override;

    float resolvePresence(TLState *tls,
                          const State& state) const override;

    float resolveRefractiveIndex(TLState *tls,
                                 const State& state) const override;

    void * getCastsCausticsISPCFunc() const override
    {
        return ispc::HairLayerMaterial_getCastsCausticsFunc();
    }

    void * getResolveSubsurfaceTypeISPCFunc() const override
    {
        return ispc::HairLayerMaterial_getResolveSubsurfaceTypeFunc();
    }

    void * getResolveParametersISPCFunc() const override
    {
        return ispc::HairLayerMaterial_getResolveParametersFunc();
    }

    void * getResolvePresenceISPCFunc() const override
    {
        return ispc::HairLayerMaterial_getResolvePresenceFunc();
    }

    void * getResolveSubsurfaceNormalISPCFunc() const override
    {
        return ispc::HairLayerMaterial_getresolveSubsurfaceNormalFunc();
    }

    EvalNormalFunc getEvalSubsurfaceNormalFunc() const override
    {
        return reinterpret_cast<EvalNormalFunc>(HairLayerMaterial::evalSubsurfaceNormal);
    }

    const ispc::HairLayerMaterial* getISPCHairLayerMaterialStruct() const { return &mIspc; }

private:
    static void shade(const scene_rdl2::rdl2::Material* self,
                      TLState *tls,
                      const State& state,
                      BsdfBuilder& bsdfBuilder);

    static float presence(const scene_rdl2::rdl2::Material* self,
                          TLState *tls,
                          const State& state);
    static float ior(const scene_rdl2::rdl2::Material* self,
                     TLState *tls,
                     const State& state);

    static scene_rdl2::math::Vec3f evalSubsurfaceNormal(const scene_rdl2::rdl2::Material* self,
                                            TLState *tls,
                                            const State& state);

    ispc::HairLayerMaterial  mIspc;
    const DwaBaseLayerable* mLayerableA;
    const DwaBaseLayerable* mLayerableB;

RDL2_DSO_CLASS_END(HairLayerMaterial)


//---------------------------------------------------------------------------

HairLayerMaterial::HairLayerMaterial(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name)
    : Parent(sceneClass, name, sLabels)
    , mLayerableA(nullptr)
    , mLayerableB(nullptr)
{
    mType |= scene_rdl2::rdl2::INTERFACE_DWABASEHAIRLAYERABLE;

    mShadeFunc      = HairLayerMaterial::shade;
    mShadeFuncv     = (scene_rdl2::rdl2::ShadeFuncv) ispc::HairLayerMaterial_getShadeFunc();
    mPresenceFunc   = HairLayerMaterial::presence;
    mIorFunc        = HairLayerMaterial::ior;

    mIspc.mEvalSubsurfaceNormal = (intptr_t)HairLayerMaterial::evalSubsurfaceNormal;

    // register shade time event messages.  we require and expect
    // these events to have the same value across all instances of the shader.
    // no conditional registration of events is allowed.
    // to allow for the possibility that we may someday create materials
    // on multiple threads, we'll protect the writes of the class statics
    // with a mutex.
    mIspc.mStaticData = (ispc::HairLayerMaterialStaticData*)&sStaticHairLayerMaterialData;

    static std::mutex errorMutex;
    std::scoped_lock lock(errorMutex);
    MOONRAY_START_THREADSAFE_STATIC_WRITE

    mIspc.mStaticData->sErrorMismatchedFresnelType =
        mLogEventRegistry.createEvent(scene_rdl2::logging::ERROR_LEVEL,
        "Hair material fresnel types do not match");

    MOONRAY_FINISH_THREADSAFE_STATIC_WRITE
}

void
HairLayerMaterial::resolveUniformParameters(ispc::DwaBaseUniformParameters &uParams) const
{
    ispc::DwaBaseUniformParameters uParamsA, uParamsB;
    mLayerableA->resolveUniformParameters(uParamsA);
    mLayerableB->resolveUniformParameters(uParamsB);
    uParams = uParamsA;
    blendUniformParameters(uParamsA,
                           uParamsB,
                           uParams,
                           1,                       // fallback specular model
                           1,                       // fallback toon specular model
                           1,                       // fallback outer specular model
                           false,                   // fallback outer specular use bending
                           get(attrFallbackBSSRDF), // only attr relevant to hair
                           false);                  // fallback thin geometry
}

void
HairLayerMaterial::update()
{
    if (hasChanged(attrHairMaterialA) || hasChanged(attrHairMaterialB)) {
        mIspc.mColorSpace = static_cast<ispc::BlendColorSpace>(get(attrColorSpace));
        mLayerableA = registerHairLayerable(get(attrHairMaterialA), mIspc.mSubMtlA);
        mLayerableB = registerHairLayerable(get(attrHairMaterialB), mIspc.mSubMtlB);

        resolveUniformParameters(mIspc.mUParams);
    }
}

bool
HairLayerMaterial::getCastsCaustics() const
{
    if (!mLayerableB && !mLayerableA) {
        return false;
    } else if (mLayerableB && mLayerableA) {
        return mLayerableA->getCastsCaustics() || mLayerableB->getCastsCaustics();
    } else if (mLayerableB) {
        return mLayerableB->getCastsCaustics();
    } else {
        return mLayerableA->getCastsCaustics();
    }
}

int
HairLayerMaterial::resolveSubsurfaceType(const State& state) const
{
    return mIspc.mUParams.mSubsurface;
}

bool
HairLayerMaterial::resolveParameters(TLState *tls,
                                     const State& state,
                                     const bool castsCaustics,
                                     ispc::DwaBaseParameters &params) const
{
    return blendHairParameters(tls,
                               state,
                               castsCaustics,
                               params,
                               mIspc.mUParams,
                               mIspc.mColorSpace,
                               mIspc.mEvalSubsurfaceNormal,
                               mLayerableB,
                               mLayerableA,
                               this,
                               mIspc.mStaticData->sErrorMismatchedFresnelType,
                               saturate(evalFloat(this, attrMask, tls, state)));
}

float
HairLayerMaterial::resolvePresence(TLState *tls,
                                   const State& state) const
{
    return blendPresence(tls,
                         state,
                         mLayerableB,
                         mLayerableA,
                         saturate(evalFloat(this, attrMask, tls, state)));
}

float
HairLayerMaterial::resolveRefractiveIndex(TLState *tls,
                                          const State& state) const
{
    return blendRefractiveIndex(tls,
                                state,
                                mLayerableB,
                                mLayerableA,
                                saturate(evalFloat(this, attrMask, tls, state)));
}

scene_rdl2::math::Vec3f
HairLayerMaterial::resolveSubsurfaceNormal(TLState *tls,
                                           const State& state) const

{
    return blendSubsurfaceNormal(tls,
                                 state,
                                 mLayerableB,
                                 mLayerableA,
                                 saturate(evalFloat(this, attrMask, tls, state)));
}

void
HairLayerMaterial::shade(const scene_rdl2::rdl2::Material* self,
                         TLState *tls,
                         const State& state,
                         BsdfBuilder& bsdfBuilder)
{
    const HairLayerMaterial* me = static_cast<const HairLayerMaterial*>(self);
    const ispc::HairLayerMaterial* ispc = me->getISPCHairLayerMaterialStruct();

    // First verify that any submaterials that are connected are
    // DwaBaseLayerable, otherwise exit early
    if (me->get(attrHairMaterialA) && !me->mLayerableA) { return; }
    if (me->get(attrHairMaterialB) && !me->mLayerableB) { return; }

    // check to see if either material casts caustics
    bool castsCaustics = false;
    if (!me->mLayerableB) {
        // exit early, background material is missing or not Layerable
        return;
    } else if (!me->mLayerableA) {
        castsCaustics = me->mLayerableB->getCastsCaustics();
    } else {
        castsCaustics = (me->mLayerableB->getCastsCaustics() ||
                me->mLayerableA->getCastsCaustics());
    }

    ispc::DwaBaseParameters params;
    if (me->resolveParameters(tls, state, castsCaustics, params)) {
        me->createLobes(me, tls, state, bsdfBuilder, params, ispc->mUParams, sLabels);
    }
}

float
HairLayerMaterial::presence(const scene_rdl2::rdl2::Material* self,
                            TLState *tls,
                            const State& state)
{
    const HairLayerMaterial* me = static_cast<const HairLayerMaterial*>(self);
    return me->resolvePresence(tls, state);
}

float
HairLayerMaterial::ior(const scene_rdl2::rdl2::Material* self,
                       TLState* tls,
                       const State& state)
{
    const HairLayerMaterial* me = static_cast<const HairLayerMaterial*>(self);
    return me->resolveRefractiveIndex(tls, state);
}

scene_rdl2::math::Vec3f
HairLayerMaterial::evalSubsurfaceNormal(const scene_rdl2::rdl2::Material* self,
                                        TLState *tls,
                                        const State& state)
{
    const HairLayerMaterial* me = static_cast<const HairLayerMaterial*>(self);
    return me->resolveSubsurfaceNormal(tls, state);
}

// This is a construct to access the ISPC Data Struct from Inside the Scalar Material object.
// Note we are not using MATERIAL_GET_ISPC_CPTR to retrieve the ispc structure from the Material
// class as in other materials since that macro assumes a certain data layout we do not adhere to in
// DwaBase materials.
extern "C" {
const void*
getHairLayerMaterialStruct(const void* ptr)
{
    const HairLayerMaterial* classPtr = static_cast<const HairLayerMaterial*>(ptr);
    return (static_cast<const void*>(classPtr->getISPCHairLayerMaterialStruct()));
}
}

//---------------------------------------------------------------------------


