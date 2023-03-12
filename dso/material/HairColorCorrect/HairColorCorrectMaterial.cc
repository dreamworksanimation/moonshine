// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

///
/// @file HairColorCorrectMaterial.cc
/// $Id$
///

#include "attributes.cc"
#include "HairColorCorrectMaterial_ispc_stubs.h"
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

RDL2_DSO_CLASS_BEGIN(HairColorCorrectMaterial, DwaBaseLayerable)

public:
    HairColorCorrectMaterial(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name);

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
        return ispc::HairColorCorrectMaterial_getCastsCausticsFunc();
    }

    void * getResolveSubsurfaceTypeISPCFunc() const override
    {
        return ispc::HairColorCorrectMaterial_getResolveSubsurfaceTypeFunc();
    }

    void * getResolveParametersISPCFunc() const override
    {
        return ispc::HairColorCorrectMaterial_getResolveParametersFunc();
    }

    void * getResolvePresenceISPCFunc() const override
    {
        return ispc::HairColorCorrectMaterial_getResolvePresenceFunc();
    }

    void * getResolveSubsurfaceNormalISPCFunc() const override
    {
        return ispc::HairColorCorrectMaterial_getresolveSubsurfaceNormalFunc();
    }

    EvalNormalFunc getEvalSubsurfaceNormalFunc() const override
    {
        return reinterpret_cast<EvalNormalFunc>(HairColorCorrectMaterial::evalSubsurfaceNormal);
    }

    const ispc::HairColorCorrectMaterial* getISPCMaterialStruct() const { return &mIspc; }

private:
    static void shade(const scene_rdl2::rdl2::Material* self,
                      TLState *tls,
                      const State& state,
                      BsdfBuilder& bsdfBuilder);

    static float presence(const scene_rdl2::rdl2::Material* self,
                          TLState *tls,
                          const State& state);

    static float ior(const scene_rdl2::rdl2::Material* self,
                     moonray::shading::TLState *tls,
                     const moonray::shading::State& state);

    static Vec3f evalSubsurfaceNormal(const scene_rdl2::rdl2::Material* self,
                                      TLState *tls,
                                      const State& state);

    void modifyParameters(moonray::shading::TLState *tls,
                          const moonray::shading::State &state,
                          ispc::DwaBaseParameters &params) const;

    ispc::HairColorCorrectMaterial mIspc;
    const DwaBaseLayerable* mInputMtl;

RDL2_DSO_CLASS_END(HairColorCorrectMaterial)


//---------------------------------------------------------------------------

HairColorCorrectMaterial::HairColorCorrectMaterial(
        const scene_rdl2::rdl2::SceneClass& sceneClass,
        const std::string& name)
    : Parent(sceneClass, name, sLabels)
    , mInputMtl(nullptr)
{
    mType |= scene_rdl2::rdl2::INTERFACE_DWABASEHAIRLAYERABLE;

    mShadeFunc = HairColorCorrectMaterial::shade;
    mShadeFuncv = (scene_rdl2::rdl2::ShadeFuncv) ispc::HairColorCorrectMaterial_getShadeFunc();
    mPresenceFunc = HairColorCorrectMaterial::presence;
    mIorFunc = HairColorCorrectMaterial::ior;
}

void
HairColorCorrectMaterial::resolveUniformParameters(ispc::DwaBaseUniformParameters &uParams) const
{
    if (mInputMtl) {
        mInputMtl->resolveUniformParameters(uParams);
    }
}

void
HairColorCorrectMaterial::update()
{
    mInputMtl = registerHairLayerable(get(attrInputMaterial), mIspc.mSubMaterial);
    resolveUniformParameters(mIspc.mUParams);
}

bool
HairColorCorrectMaterial::getCastsCaustics() const
{
    if (mInputMtl) {
        return mInputMtl->getCastsCaustics();
    } else {
        return false;
    }
}

bool
HairColorCorrectMaterial::resolveParameters(TLState *tls,
                                           const State& state,
                                           const bool castsCaustics,
                                           ispc::DwaBaseParameters &params) const
{
    bool result = false;
    if (mInputMtl) {
        result = mInputMtl->resolveParameters(tls, state, castsCaustics, params);
        modifyParameters(tls, state, params);
    }
    return result;
}

Vec3f
HairColorCorrectMaterial::resolveSubsurfaceNormal(TLState *tls,
                                                 const State& state) const
{
    Vec3f result(state.getN());
    if (mInputMtl) {
        result = mInputMtl->resolveSubsurfaceNormal(tls, state);
    }
    return result;
}

int
HairColorCorrectMaterial::resolveSubsurfaceType(const State& state) const
{
    int type = ispc::SubsurfaceType::SUBSURFACE_NONE;
    if (mInputMtl) {
        type = mInputMtl->resolveSubsurfaceType(state);
    }
    return type;
}

float
HairColorCorrectMaterial::resolvePresence(TLState *tls,
                                         const State& state) const
{
    float result = 1.f;
    if (mInputMtl) {
        result = mInputMtl->resolvePresence(tls, state);
    }
    return result;
}

float
HairColorCorrectMaterial::resolveRefractiveIndex(TLState *tls,
                                                const State& state) const
{
    float result = 1.f;
    if (mInputMtl) {
        result = mInputMtl->resolveRefractiveIndex(tls, state);
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
HairColorCorrectMaterial::modifyParameters(moonray::shading::TLState *tls,
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

    #define NUM_COLOR_ARRAY_ELEMS 3
    Color * colors[NUM_COLOR_ARRAY_ELEMS] = {
        asCpp(&params.mHairParameters.mHairColor),
        asCpp(&params.mHairParameters.mHairDiffuseFrontColor),
        asCpp(&params.mHairParameters.mHairDiffuseBackColor)
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
HairColorCorrectMaterial::shade(const scene_rdl2::rdl2::Material* self,
                                TLState *tls,
                                const State& state,
                                BsdfBuilder& bsdfBuilder)
{
    const HairColorCorrectMaterial* me = static_cast<const HairColorCorrectMaterial*>(self);
    const ispc::HairColorCorrectMaterial* ispc = me->getISPCMaterialStruct();
    const bool castsCaustics = me->getCastsCaustics();

    ispc::DwaBaseParameters params;

    if (me->resolveParameters(tls, state, castsCaustics, params)) {
        me->createLobes(me, tls, state, bsdfBuilder, params, ispc->mUParams, sLabels);
    }
}

float
HairColorCorrectMaterial::presence(const scene_rdl2::rdl2::Material* self,
                                  TLState *tls,
                                  const State& state)
{
    const HairColorCorrectMaterial* me = static_cast<const HairColorCorrectMaterial*>(self);
    return me->resolvePresence(tls, state);
}

float
HairColorCorrectMaterial::ior(const scene_rdl2::rdl2::Material* self,
                             TLState* tls,
                             const State& state)
{
    const HairColorCorrectMaterial* me = static_cast<const HairColorCorrectMaterial*>(self);
    return me->resolveRefractiveIndex(tls, state);
}

Vec3f
HairColorCorrectMaterial::evalSubsurfaceNormal(const scene_rdl2::rdl2::Material* self,
                                               TLState *tls,
                                               const State& state)
{
    const HairColorCorrectMaterial* me = static_cast<const HairColorCorrectMaterial*>(self);
    return me->resolveSubsurfaceNormal(tls, state);
}

// This is a construct to access the ISPC Data Struct from Inside the Scalar Material object.
// Note we are not using MATERIAL_GET_ISPC_CPTR to retrieve the ispc structure from the Material
// class as in other materials since that macro assumes a certain data layout we do not adhere to in
// DwaBase materials.
extern "C" {
const void*
getHairColorCorrectMaterialStruct(const void* ptr)
{
    const HairColorCorrectMaterial* classPtr = static_cast<const HairColorCorrectMaterial*>(ptr);
    return (static_cast<const void*>(classPtr->getISPCMaterialStruct()));
}

}
//---------------------------------------------------------------------------

