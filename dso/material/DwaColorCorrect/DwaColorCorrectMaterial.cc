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
#include <moonray/rendering/shading/ColorCorrect.h>
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

    const ispc::DwaColorCorrectMaterial* getISPCMaterialStruct() const { return &mIspc; }

private:
    static void shade(const scene_rdl2::rdl2::Material* self,
                      TLState *tls,
                      const State& state,
                      BsdfBuilder& bsdfBuilder);

    static Vec3f evalSubsurfaceNormal(const scene_rdl2::rdl2::Material* self,
                                      TLState *tls,
                                      const State& state);

    void modifyParameters(moonray::shading::TLState *tls,
                          const moonray::shading::State &state,
                          ispc::DwaBaseParameters &params) const;

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
    mType |= scene_rdl2::rdl2::INTERFACE_DWABASELAYERABLE;

    mShadeFunc = DwaColorCorrectMaterial::shade;
    mShadeFuncv = (scene_rdl2::rdl2::ShadeFuncv) ispc::DwaColorCorrectMaterial_getShadeFunc();
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
        result = mInputMtl->resolveParameters(tls, state, castsCaustics, params);
        if (result) {
            modifyParameters(tls, state, params);
        }
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

static void
clampTo0(Color& c)
{
    c.r = max(0.f, c.r);
    c.g = max(0.f, c.g);
    c.b = max(0.f, c.b);
}

static void
clampTo1(Color& c)
{
        c.r = min(1.f, c.r);
        c.g = min(1.f, c.g);
        c.b = min(1.f, c.b);
}

static void
applyColorCorrections(const float hueShift,
                      const float saturation,
                      const float gainValue,
                      const bool tmiEnabled,
                      const Color& tmi,
                      Color &c)
{
    if (!isZero(hueShift)) {
        moonray::shading::applyHueShift(hueShift, c);
    }

    if (!isOne(saturation)) {
        moonray::shading::applySaturation(saturation, c);
    }

    c = c * gainValue;

    if (tmiEnabled) {
        moonray::shading::applyTMI(tmi, c);
    }
}

void
DwaColorCorrectMaterial::modifyParameters(moonray::shading::TLState *tls,
                                          const moonray::shading::State &state,
                                          ispc::DwaBaseParameters &params) const
{
    if (!get(attrOn)) { return; }

    const float mix = saturate(evalFloat(this, attrMix, tls, state));
    if (isZero(mix)) { return; }

    const float hueShift = evalFloat(this, attrHueShift, tls, state);
    const float saturation = max(0.f, evalFloat(this, attrSaturation, tls, state));
    const float gainValue = evalFloat(this, attrGain, tls, state);
    const bool tmiEnabled = get(attrTMIEnabled);
    const Color tmi = get(attrTMI);

    // We'll apply color correction to these particular params.
    // Use pointer array to allow for processing them in a loop.
    // We'll also apply color correction to emission later....

    #define NUM_COLOR_ARRAY_ELEMS 8
    Color * colors[NUM_COLOR_ARRAY_ELEMS] = {
        asCpp(&params.mFuzzAlbedo),
        asCpp(&params.mMetallicColor),
        asCpp(&params.mMetallicEdgeColor),
        asCpp(&params.mWarpColor),
        asCpp(&params.mWeftColor),
        asCpp(&params.mTransmissionColor),
        asCpp(&params.mAlbedo),
        asCpp(&params.mDiffuseTransmission)
    };

    // process refl/trans values
    for (uint8_t i = 0; i < NUM_COLOR_ARRAY_ELEMS; ++i) {
        Color& c = *colors[i];
        const Color original = c;
        applyColorCorrections(hueShift,
                              saturation,
                              gainValue,
                              tmiEnabled,
                              tmi,
                              c);
        c = lerpOpt(original, c, mix);

        // clamp refl/trans vals to [0,1]
        clampTo0(c);
        clampTo1(c);
    }

    // handle toon ramp, if present
    if (!isZero(params.mToonDiffuseParams.mToonDiffuse) && 
        params.mToonDiffuseParams.mModel == ispc::TOON_DIFFUSE_RAMP) {
        const int rampPts = params.mToonDiffuseParams.mRampNumPoints;
        for (uint8_t i = 0; i < rampPts; ++i) {
            Color c = asCpp(params.mToonDiffuseParams.mRampColors[i]);
            const Color original = c;
            applyColorCorrections(hueShift,
                                  saturation,
                                  gainValue,
                                  tmiEnabled,
                                  tmi,
                                  c);
            c = lerpOpt(original, c, mix);

            // clamp refl/trans vals to [0,1]
            clampTo0(c);
            clampTo1(c);
            asCpp(params.mToonDiffuseParams.mRampColors[i]) = c;
        }
    }

    // handle emission separately, don't clamp at 1.0
    {
        Color& c = asCpp(params.mEmission);
        const Color original = c;
        applyColorCorrections(hueShift,
                              saturation,
                              gainValue,
                              tmiEnabled,
                              tmi,
                              c);
        c = lerpOpt(original, c, mix);

        // clamp emission to [0,inf]
        clampTo0(c);
    }
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

    if (me->resolveParameters(tls, state, castsCaustics, params)) {
        me->createLobes(me, tls, state, bsdfBuilder, params, ispc->mUParams, sLabels);
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


