// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

///
/// @file DwaLayerMaterial.cc
/// $Id$
///

#include "attributes.cc"
#include "DwaLayerMaterial_ispc_stubs.h"
#include "labels.h"

#include <moonray/rendering/shading/MaterialApi.h>
#include <moonshine/material/dwabase/DwaBase.h>
#include <moonshine/material/dwabase/DwaBaseLayerable.h>
#include <moonshine/material/dwabase/Blending.h>

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

    // These parameters exist for when both input materials are glitter
    // in which case we construct a Glitter object that is owned by this
    // layer material and overrides the uniform parameters of the input
    // materials glitter.
    keys.mGlitterSeed                           = attrFallbackGlitterSeed;
    keys.mGlitterSpace                          = attrFallbackGlitterSpace;
    keys.mGlitterRandomness                     = attrFallbackGlitterRandomness;
    keys.mGlitterStyleAFrequency                = attrFallbackGlitterStyleAFrequency;
    keys.mGlitterFlakeTextureA                  = attrFallbackGlitterTextureA;
    keys.mGlitterStyleBFrequency                = attrFallbackGlitterStyleBFrequency;
    keys.mGlitterFlakeTextureB                  = attrFallbackGlitterTextureB;
    keys.mGlitterDenseLodQuality                = attrFallbackGlitterLodQuality;
    keys.mGlitterLayeringMode                   = attrFallbackGlitterLayeringMode;
    keys.mGlitterDebugMode                      = attrFallbackGlitterDebugMode;

    return keys;
}

} // end anonymous namespace

//---------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(DwaLayerMaterial, DwaBase)

public:
    DwaLayerMaterial(const SceneClass& sceneClass, const std::string& name);

    void resolveUniformParameters(ispc::DwaBaseUniformParameters &uParams) const override;

    virtual void update() override;

    bool hasGlitter() const override;

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

    bool resolvePreventLightCulling(const State& state) const override;

    void * getCastsCausticsISPCFunc() const override
    {
        return ispc::DwaLayerMaterial_getCastsCausticsFunc();
    }

    void * getResolveSubsurfaceTypeISPCFunc() const override
    {
        return ispc::DwaLayerMaterial_getResolveSubsurfaceTypeFunc();
    }

    void * getResolveParametersISPCFunc() const override
    {
        return ispc::DwaLayerMaterial_getResolveParametersFunc();
    }

    void * getResolvePresenceISPCFunc() const override
    {
        return ispc::DwaLayerMaterial_getResolvePresenceFunc();
    }

    void * getResolvePreventLightCullingISPCFunc() const override
    {
        return ispc::DwaLayerMaterial_getResolvePreventLightCullingFunc();
    }

    void * getResolveSubsurfaceNormalISPCFunc() const override
    {
        return ispc::DwaLayerMaterial_getresolveSubsurfaceNormalFunc();
    }

    EvalNormalFunc getEvalSubsurfaceNormalFunc() const override
    {
        return reinterpret_cast<EvalNormalFunc>(DwaLayerMaterial::evalSubsurfaceNormal);
    }

    const ispc::DwaLayerMaterial* getISPCLayerMaterialStruct() const { return &mIspc; }

private:
    static void shade(const Material* self,
                      TLState *tls,
                      const State& state,
                      BsdfBuilder& bsdfBuilder);

    static Vec3f evalSubsurfaceNormal(const Material* self,
                                      TLState *tls,
                                      const State& state);

    ispc::DwaLayerMaterial  mIspc;
    const DwaBaseLayerable* mLayerableA;
    const DwaBaseLayerable* mLayerableB;

RDL2_DSO_CLASS_END(DwaLayerMaterial)


//---------------------------------------------------------------------------

DwaLayerMaterial::DwaLayerMaterial(const SceneClass& sceneClass, const std::string& name)
    : DwaBase(sceneClass, name,
              collectAttributeKeys(),
              ispc::DwaLayerMaterial_collectAttributeFuncs() ,
              sLabels,
              ispc::Model::Layer)
    , mLayerableA(nullptr)
    , mLayerableB(nullptr)
{
    mType |= INTERFACE_DWABASELAYERABLE;

    mShadeFunc = DwaLayerMaterial::shade;
    mShadeFuncv = (ShadeFuncv) ispc::DwaLayerMaterial_getShadeFunc();
    mIspc.mEvalSubsurfaceNormal = (intptr_t)DwaLayerMaterial::evalSubsurfaceNormal;

    // This is used to get the glitter pointer and uniform parameters in ispc
    mIspc.mDwaBase = getISPCBaseMaterialStruct();
}

void
DwaLayerMaterial::resolveUniformParameters(ispc::DwaBaseUniformParameters &uParams) const
{
    if (mLayerableA && mLayerableB) {
        ispc::DwaBaseUniformParameters uParamsA, uParamsB;
        mLayerableA->resolveUniformParameters(uParamsA);
        mLayerableB->resolveUniformParameters(uParamsB);
        blendUniformParameters(uParamsA,
                               uParamsB,
                               uParams,
                               get(attrFallbackSpecularModel),
                               get(attrFallbackToonSpecularModel),
                               get(attrFallbackOuterSpecularModel),
                               get(attrFallbackOuterSpecularUseBending),
                               get(attrFallbackBSSRDF),
                               get(attrFallbackThinGeometry));
    } else if (!mLayerableA && mLayerableB) {
        mLayerableB->resolveUniformParameters(uParams);
    }
}

void
DwaLayerMaterial::update()
{
    if (hasChanged(attrMaterialA) || hasChanged(attrMaterialB)) {
        mIspc.mColorSpace = static_cast<ispc::BlendColorSpace>(get(attrColorSpace));

        if (get(attrMaterialB) == nullptr) {
            fatal("No material_B specified");
            return;
        }

        mLayerableA = registerLayerable(get(attrMaterialA), mIspc.mSubMtlA);
        mLayerableB = registerLayerable(get(attrMaterialB), mIspc.mSubMtlB);

        // Verify that any submaterials that are connected are
        // DwaBaseLayerable, otherwise exit early
        if (get(attrMaterialA) && !mLayerableA) {
            fatal("material_A is not layerable");
            return;
        }
        if (get(attrMaterialB) && !mLayerableB) {
            fatal("material_B is not layerable");
            return;
        }

        resolveUniformParameters(mIspc.mUParams);

        // If both input materials have glitter enabled then we construct
        // a new Glitter object which is used to override the input materials'
        // uniform glitter parameters.   This is needed because we create one
        // set of blended glitter lobes and because the uniform parameters
        // are set at construction time they can't be blended at shade time.
        if (mIspc.mSubMtlA.mHasGlitter && mIspc.mSubMtlB.mHasGlitter) {
            updateGlitter();
        }
    }

    mIspc.mSubsurfaceTraceSet = (TraceSet *)get(attrSubsurfaceTraceSet);
}

bool
DwaLayerMaterial::hasGlitter() const
{
    return mIspc.mSubMtlA.mHasGlitter || mIspc.mSubMtlB.mHasGlitter;
}

bool
DwaLayerMaterial::getCastsCaustics() const
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


bool
DwaLayerMaterial::resolvePreventLightCulling(const State& state) const
{
    return mIspc.mDwaBase->mUParams.mPreventLightCulling;
}

int
DwaLayerMaterial::resolveSubsurfaceType(const State& state) const
{
    return mIspc.mDwaBase->mUParams.mSubsurface;
}

bool
DwaLayerMaterial::resolveParameters(TLState *tls,
                                    const State& state,
                                    const bool castsCaustics,
                                    ispc::DwaBaseParameters &params) const
{
    return blendParameters(tls,
                           state,
                           castsCaustics,
                           params,
                           mIspc.mDwaBase->mUParams,
                           mIspc.mColorSpace,
                           getGlitterPointer(),
                           mIspc.mEvalSubsurfaceNormal,
                           mIspc.mSubMtlB.mHasGlitter,
                           mIspc.mSubMtlA.mHasGlitter,
                           mLayerableB,
                           mLayerableA,
                           saturate(evalFloat(this, attrMask, tls, state)));
}

float
DwaLayerMaterial::resolvePresence(TLState *tls,
                                  const State& state) const
{
    return blendPresence(tls,
                         state,
                         mLayerableB,
                         mLayerableA,
                         saturate(evalFloat(this, attrMask, tls, state)));
}

float
DwaLayerMaterial::resolveRefractiveIndex(TLState *tls,
                                         const State& state) const
{
    return blendRefractiveIndex(tls,
                                state,
                                mLayerableB,
                                mLayerableA,
                                saturate(evalFloat(this, attrMask, tls, state)));
}

Vec3f
DwaLayerMaterial::resolveSubsurfaceNormal(TLState *tls,
                                          const State& state) const
{
    return blendSubsurfaceNormal(tls,
                                 state,
                                 mLayerableB,
                                 mLayerableA,
                                 saturate(evalFloat(this, attrMask, tls, state)));
}


void
DwaLayerMaterial::shade(const Material* self,
                        TLState *tls,
                        const State& state,
                        BsdfBuilder& bsdfBuilder)
{
    const DwaLayerMaterial* me = static_cast<const DwaLayerMaterial*>(self);
    const ispc::DwaLayerMaterial* ispc = me->getISPCLayerMaterialStruct();

    const bool castsCaustics = me->getCastsCaustics();

    ispc::DwaBaseParameters params;
    if (me->resolveParameters(tls, state, castsCaustics, params)) {
        // get sss trace set for DwaLayerMaterial
        params.mSubsurfaceTraceSet = static_cast<TraceSet *>(me->get(attrSubsurfaceTraceSet));
        me->createLobes(me, tls, state, bsdfBuilder, params, ispc->mUParams, sLabels);
    }
}

Vec3f
DwaLayerMaterial::evalSubsurfaceNormal(const Material* self,
                                       TLState *tls,
                                       const State& state)
{
    const DwaLayerMaterial* me = static_cast<const DwaLayerMaterial*>(self);
    return me->resolveSubsurfaceNormal(tls, state);
}



// This is a construct to access the ISPC Data Struct from Inside the Scalar Material object.
// Note we are not using MATERIAL_GET_ISPC_CPTR to retrieve the ispc structure from the Material
// class as in other materials since that macro assumes a certain data layout we do not adhere to in
// DwaBase materials.
extern "C" {
const void*
getDwaLayerMaterialStruct(const void* ptr)
{
    const DwaLayerMaterial* classPtr = static_cast<const DwaLayerMaterial*>(ptr);
    return (static_cast<const void*>(classPtr->getISPCLayerMaterialStruct()));
}
}

//---------------------------------------------------------------------------


