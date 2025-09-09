// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

///
/// @file DwaColorCorrectMaterial.cc
/// $Id$
///

#include "attributes.cc"
#include "DwaColorCorrectMaterial_ispc_stubs.h"
#include "labels.h"

#include <moonshine/material/dwabase/DwaBase.h>

#include <moonray/common/mcrt_macros/moonray_static_check.h>
#include <moonray/rendering/shading/MaterialApi.h>
#include <moonshine/material/dwabase/Blending.h>

#include <string>

using namespace scene_rdl2::math;
using namespace moonray::shading;
using namespace moonshine::dwabase;

namespace {

DECLARE_DWA_BASE_LABELS()

} // end anonymous namespace

//---------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(DwaColorCorrectMaterial, DwaBaseLayerable)

public:
    DwaColorCorrectMaterial(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name);

    void resolveUniformParameters(ispc::DwaBaseUniformParameters &uParams) const override;

    virtual void update() override;

    bool getCastsCaustics() const override;

    bool hasGlitter() const override { return mInputMtl->hasGlitter(); }

    int resolveSubsurfaceType(const State& state) const override;

    bool resolveParameters(TLState *tls,
                           const State& state,
                           bool castsCaustics,
                           ispc::DwaBaseParameters &params) const override;

    virtual void createLobes(const DwaBaseLayerable * const dwaBaseLayerable,
                             moonray::shading::TLState *tls,
                             const moonray::shading::State &state,
                             moonray::shading::BsdfBuilder &builder,
                             const ispc::DwaBaseParameters &params,
                             const ispc::DwaBaseUniformParameters &uParams,
                             const ispc::DwaBaseLabels &labels) const override;

    Vec3f resolveSubsurfaceNormal(TLState *tls,
                              const State& state) const override;

    float resolvePresence(TLState *tls,
                          const State& state) const override;

    float resolveRefractiveIndex(TLState *tls,
                                 const State& state) const override;

    bool resolvePreventLightCulling(const State& state) const override;

    void * getCastsCausticsISPCFunc() const override
    {
        return ispc::DwaColorCorrectMaterial_getCastsCausticsFunc();
    }

    void * getResolveSubsurfaceTypeISPCFunc() const override
    {
        return ispc::DwaColorCorrectMaterial_getResolveSubsurfaceTypeFunc();
    }

    void * getResolveParametersISPCFunc() const override
    {
        return ispc::DwaColorCorrectMaterial_getResolveParametersFunc();
    }

    void * getResolvePresenceISPCFunc() const override
    {
        return ispc::DwaColorCorrectMaterial_getResolvePresenceFunc();
    }

    void * getResolvePreventLightCullingISPCFunc() const override
    {
        return ispc::DwaColorCorrectMaterial_getResolvePreventLightCullingFunc();
    }

    void * getResolveSubsurfaceNormalISPCFunc() const override
    {
        return ispc::DwaColorCorrectMaterial_getresolveSubsurfaceNormalFunc();
    }

    EvalNormalFunc getEvalSubsurfaceNormalFunc() const override
    {
        return reinterpret_cast<EvalNormalFunc>(DwaColorCorrectMaterial::evalSubsurfaceNormal);
    }

    void * getCreateLobesISPCFunc() const override
    {
        return ispc::DwaColorCorrectMaterial_getCreateLobesFunc();
    }

    const ispc::DwaColorCorrectMaterial* getISPCMaterialStruct() const { return &mIspc; }

private:
    static void shade(const scene_rdl2::rdl2::Material* self,
                      TLState *tls,
                      const State& state,
                      BsdfBuilder& bsdfBuilder);

    static Vec3f evalSubsurfaceNormal(const scene_rdl2::rdl2::Material* self,
                                      TLState *tls,
                                      const State& state);

    ispc::DwaColorCorrectMaterial mIspc;
    const DwaBaseLayerable* mInputMtl;

RDL2_DSO_CLASS_END(DwaColorCorrectMaterial)


//---------------------------------------------------------------------------

DwaColorCorrectMaterial::DwaColorCorrectMaterial(
        const scene_rdl2::rdl2::SceneClass& sceneClass,
        const std::string& name)
    : Parent(sceneClass, name, sLabels)
    , mInputMtl(nullptr)
{
    mType |= scene_rdl2::rdl2::INTERFACE_DWABASE;
    mType |= scene_rdl2::rdl2::INTERFACE_DWABASELAYERABLE;

    mShadeFunc = DwaColorCorrectMaterial::shade;
    mShadeFuncv = (scene_rdl2::rdl2::ShadeFuncv) ispc::DwaColorCorrectMaterial_getShadeFunc();
    mIspc.mEvalSubsurfaceNormal = (intptr_t)DwaColorCorrectMaterial::evalSubsurfaceNormal;
}

void
DwaColorCorrectMaterial::resolveUniformParameters(ispc::DwaBaseUniformParameters &uParams) const
{
    if (mInputMtl) {
        mInputMtl->resolveUniformParameters(uParams);
    }
}

void
DwaColorCorrectMaterial::update()
{
    mInputMtl = registerLayerable(get(attrInputMaterial), mIspc.mSubMaterial);
    resolveUniformParameters(mIspc.mUParams);
}

bool
DwaColorCorrectMaterial::getCastsCaustics() const
{
    if (mInputMtl) {
        return mInputMtl->getCastsCaustics();
    } else {
        return false;
    }
}

int
DwaColorCorrectMaterial::resolveSubsurfaceType(const State& state) const
{
    int type = ispc::SubsurfaceType::SUBSURFACE_NONE;
    if (mInputMtl) {
        type = mInputMtl->resolveSubsurfaceType(state);
    }
    return type;
}

bool
DwaColorCorrectMaterial::resolveParameters(TLState *tls,
                                           const State& state,
                                           const bool castsCaustics,
                                           ispc::DwaBaseParameters &params) const
{
    bool result = false;
    if (mInputMtl) {
        const size_t cc = params.mNumColorCorrections;
        params.mColorCorrectParams[cc].mOn = get(attrOn);
        params.mColorCorrectParams[cc].mMix = saturate(evalFloat(this, attrMix, tls, state));
        params.mColorCorrectParams[cc].mHueShift = evalFloat(this, attrHueShift, tls, state);
        params.mColorCorrectParams[cc].mSaturation = max(0.f, evalFloat(this, attrSaturation, tls, state));
        params.mColorCorrectParams[cc].mGain = evalFloat(this, attrGain, tls, state);
        params.mColorCorrectParams[cc].mTmiEnabled = get(attrTMIEnabled);
        asCpp(params.mColorCorrectParams[cc].mTmi) = evalColor(this, attrTMI, tls, state);
        params.mNumColorCorrections++;

        params.mNumColorCorrections = min(ispc::DWABASE_MAX_COLOR_CORRECTIONS,
                                          params.mNumColorCorrections++);

        result = mInputMtl->resolveParameters(tls, state, castsCaustics, params);

        // override this, to make sure *this* material's evalSubsurfaceNormal() func is called,
        // and not the child material's func. Also see DwaSwitchMaterial.
        params.mEvalSubsurfaceNormalFn = mIspc.mEvalSubsurfaceNormal;
    }
    return result;
}

float
DwaColorCorrectMaterial::resolvePresence(TLState *tls,
                                         const State& state) const
{
    float result = 1.f;
    if (mInputMtl) {
        result = mInputMtl->resolvePresence(tls, state);
    }
    return result;
}

float
DwaColorCorrectMaterial::resolveRefractiveIndex(TLState *tls,
                                                const State& state) const
{
    float result = 1.f;
    if (mInputMtl) {
        result = mInputMtl->resolveRefractiveIndex(tls, state);
    }
    return result;
}

bool
DwaColorCorrectMaterial::resolvePreventLightCulling(const State& state) const
{
    bool result = false;
    if (mInputMtl) {
        result = mInputMtl->resolvePreventLightCulling(state);
    }
    return result;
}

Vec3f
DwaColorCorrectMaterial::resolveSubsurfaceNormal(TLState *tls,
                                                 const State& state) const
{
    Vec3f result(state.getN());
    if (mInputMtl) {
        result = mInputMtl->resolveSubsurfaceNormal(tls, state);
    }
    return result;
}

void
DwaColorCorrectMaterial::createLobes(const moonshine::dwabase::DwaBaseLayerable * const dwaBaseLayerable,
                                     moonray::shading::TLState *tls,
                                     const moonray::shading::State &state,
                                     moonray::shading::BsdfBuilder &bsdfBuilder,
                                     const ispc::DwaBaseParameters &params,
                                     const ispc::DwaBaseUniformParameters &uParams,
                                     const ispc::DwaBaseLabels &labels) const
{
    const DwaColorCorrectMaterial* me = static_cast<const DwaColorCorrectMaterial*>(this);
    const ispc::DwaColorCorrectMaterial* ispc = me->getISPCMaterialStruct();
    me->mInputMtl->createLobes(me, tls, state, bsdfBuilder, params, ispc->mUParams, sLabels);
}

void
DwaColorCorrectMaterial::shade(const scene_rdl2::rdl2::Material* self,
                               TLState *tls,
                               const State& state,
                               BsdfBuilder& bsdfBuilder)
{
    const DwaColorCorrectMaterial* me = static_cast<const DwaColorCorrectMaterial*>(self);
    const ispc::DwaColorCorrectMaterial* ispc = me->getISPCMaterialStruct();
    const bool castsCaustics = me->getCastsCaustics();

    ispc::DwaBaseParameters params;
    initColorCorrectParameters(params);

    if (me->resolveParameters(tls, state, castsCaustics, params)) {
        me->mInputMtl->createLobes(me, tls, state, bsdfBuilder, params, ispc->mUParams, sLabels);
    }
}

Vec3f
DwaColorCorrectMaterial::evalSubsurfaceNormal(const scene_rdl2::rdl2::Material* self,
                                              TLState *tls,
                                              const State& state)
{
    const DwaColorCorrectMaterial* me = static_cast<const DwaColorCorrectMaterial*>(self);
    return me->resolveSubsurfaceNormal(tls, state);
}



// This is a construct to access the ISPC Data Struct from Inside the Scalar Material object.
// Note we are not using MATERIAL_GET_ISPC_CPTR to retrieve the ispc structure from the Material
// class as in other materials since that macro assumes a certain data layout we do not adhere to in
// DwaBase materials.
extern "C" {
const void*
getDwaColorCorrectMaterialStruct(const void* ptr)
{
    const DwaColorCorrectMaterial* classPtr = static_cast<const DwaColorCorrectMaterial*>(ptr);
    return (static_cast<const void*>(classPtr->getISPCMaterialStruct()));
}
}

//---------------------------------------------------------------------------


