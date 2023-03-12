// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

///
/// @file DwaAdjustMaterial.cc
/// $Id$
///

#include "attributes.cc"
#include "DwaAdjustMaterial_ispc_stubs.h"
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

RDL2_DSO_CLASS_BEGIN(DwaAdjustMaterial, DwaBaseLayerable)

public:
    DwaAdjustMaterial(const SceneClass& sceneClass, const std::string& name);

    void resolveUniformParameters(ispc::DwaBaseUniformParameters &uParams) const override;

    void update() override;

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
        return ispc::DwaAdjustMaterial_getCastsCausticsFunc();
    }

    void * getResolveSubsurfaceTypeISPCFunc() const override
    {
        return ispc::DwaAdjustMaterial_getResolveSubsurfaceTypeFunc();
    }

    void * getResolveParametersISPCFunc() const override
    {
        return ispc::DwaAdjustMaterial_getResolveParametersFunc();
    }

    void * getResolvePresenceISPCFunc() const override
    {
        return ispc::DwaAdjustMaterial_getResolvePresenceFunc();
    }

    void * getResolvePreventLightCullingISPCFunc() const override
    {
        return ispc::DwaAdjustMaterial_getResolvePreventLightCullingFunc();
    }

    void * getResolveSubsurfaceNormalISPCFunc() const override
    {
        return ispc::DwaAdjustMaterial_getresolveSubsurfaceNormalFunc();
    }

    EvalNormalFunc getEvalSubsurfaceNormalFunc() const override
    {
        return reinterpret_cast<EvalNormalFunc>(DwaAdjustMaterial::evalSubsurfaceNormal);
    }

    const ispc::DwaAdjustMaterial* getISPCMaterialStruct() const { return &mIspc; }

private:
    static void shade(const Material* self,
                      TLState *tls,
                      const State& state,
                      BsdfBuilder& bsdfBuilder);

    static Vec3f evalSubsurfaceNormal(const Material* self,
                                      TLState *tls,
                                      const State& state);

    void modifyUniformParameters(ispc::DwaBaseUniformParameters &uParams) const;

    void modifyParameters(TLState *tls,
                          const State &state,
                          ispc::DwaBaseParameters &params) const;

    ispc::DwaAdjustMaterial mIspc;
    const DwaBaseLayerable* mInputMtl;

RDL2_DSO_CLASS_END(DwaAdjustMaterial)

//---------------------------------------------------------------------------

DwaAdjustMaterial::DwaAdjustMaterial(
        const SceneClass& sceneClass,
        const std::string& name)
    : Parent(sceneClass, name, sLabels)
    , mInputMtl(nullptr)
{
    mType |= INTERFACE_DWABASELAYERABLE;

    mShadeFunc = DwaAdjustMaterial::shade;
    mShadeFuncv = (ShadeFuncv) ispc::DwaAdjustMaterial_getShadeFunc();
}

void
DwaAdjustMaterial::resolveUniformParameters(ispc::DwaBaseUniformParameters &uParams) const
{
    if (mInputMtl) {
        mInputMtl->resolveUniformParameters(uParams);
        modifyUniformParameters(uParams);
    }
}

void
DwaAdjustMaterial::modifyUniformParameters(ispc::DwaBaseUniformParameters &uParams) const
{
    const int thinGeoOverride = get(attrOverrideThinGeometry);
    if (thinGeoOverride == ispc::AdjustOverrideMode::OVERRIDE_TRUE) {
        uParams.mThinGeometry = true;
    } else if (thinGeoOverride == ispc::AdjustOverrideMode::OVERRIDE_FALSE) {
        uParams.mThinGeometry = false;
    }
}

void
DwaAdjustMaterial::update()
{
    mInputMtl = registerLayerable(get(attrInputMaterial), mIspc.mSubMaterial);
    resolveUniformParameters(mIspc.mUParams);

    mOptionalAttributes.clear();

    if (get(attrEnableSpecular)) {
        mIspc.mSpecularSet       = TypedAttributeKey<float>("specular_set");
        mIspc.mSpecularSetBlend  = TypedAttributeKey<float>("specular_set_blend");
        mIspc.mSpecularMult      = TypedAttributeKey<float>("specular_mult");

        mOptionalAttributes.push_back(mIspc.mSpecularSet);
        mOptionalAttributes.push_back(mIspc.mSpecularSetBlend);
        mOptionalAttributes.push_back(mIspc.mSpecularMult);
    }

    if (get(attrEnableRoughness)) {
        mIspc.mRoughnessSet         = TypedAttributeKey<float>("roughness_set");
        mIspc.mRoughnessSetBlend    = TypedAttributeKey<float>("roughness_set_blend");
        mIspc.mRoughnessMult        = TypedAttributeKey<float>("roughness_mult");
        mIspc.mRoughnessRemapInMin  = TypedAttributeKey<float>("roughness_remap_in_min");
        mIspc.mRoughnessRemapInMax  = TypedAttributeKey<float>("roughness_remap_in_max");
        mIspc.mRoughnessRemapOutMin = TypedAttributeKey<float>("roughness_remap_out_min");
        mIspc.mRoughnessRemapOutMax = TypedAttributeKey<float>("roughness_remap_out_max");

        mOptionalAttributes.push_back(mIspc.mRoughnessSet);
        mOptionalAttributes.push_back(mIspc.mRoughnessSetBlend);
        mOptionalAttributes.push_back(mIspc.mRoughnessMult);
        mOptionalAttributes.push_back(mIspc.mRoughnessRemapInMin);
        mOptionalAttributes.push_back(mIspc.mRoughnessRemapInMax);
        mOptionalAttributes.push_back(mIspc.mRoughnessRemapOutMin);
        mOptionalAttributes.push_back(mIspc.mRoughnessRemapOutMax);
    }

    if (get(attrEnablePresence)) {
        mIspc.mPresenceSet      = TypedAttributeKey<float>("presence_set");
        mIspc.mPresenceSetBlend = TypedAttributeKey<float>("presence_set_blend");
        mIspc.mPresenceMult     = TypedAttributeKey<float>("presence_mult");

        mOptionalAttributes.push_back(mIspc.mPresenceSet);
        mOptionalAttributes.push_back(mIspc.mPresenceSetBlend);
        mOptionalAttributes.push_back(mIspc.mPresenceMult);
    }

    if (get(attrEnableColor)) {
        mIspc.mColorHueShift   = TypedAttributeKey<float>("color_hue_shift");
        mIspc.mColorSaturation = TypedAttributeKey<float>("color_saturation");
        mIspc.mColorGain       = TypedAttributeKey<float>("color_gain");
        mIspc.mColorGainRGB    = TypedAttributeKey<Color>("color_gain");

        mOptionalAttributes.push_back(mIspc.mColorHueShift);
        mOptionalAttributes.push_back(mIspc.mColorSaturation);
        mOptionalAttributes.push_back(mIspc.mColorGain);
        mOptionalAttributes.push_back(mIspc.mColorGainRGB);
    }
}

bool
DwaAdjustMaterial::getCastsCaustics() const
{
    if (mInputMtl) {
        const int castsCausticsOverride = get(attrOverrideCastsCaustics);

        if (castsCausticsOverride == ispc::AdjustOverrideMode::OVERRIDE_KEEP) {
            return mInputMtl->getCastsCaustics();
        } else {
            return castsCausticsOverride == ispc::AdjustOverrideMode::OVERRIDE_TRUE;
        }
    } else {
        return false;
    }
}

int
DwaAdjustMaterial::resolveSubsurfaceType(const State& state) const
{
    int type = ispc::SubsurfaceType::SUBSURFACE_NONE;
    if (mInputMtl) {
        type = mInputMtl->resolveSubsurfaceType(state);
    }
    return type;
}

bool
DwaAdjustMaterial::resolveParameters(TLState *tls,
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
DwaAdjustMaterial::resolvePresence(TLState *tls,
                                   const State& state) const
{
    float result = 1.f;

    if (mInputMtl) {
        result = mInputMtl->resolvePresence(tls, state);

        if (!get(attrOn)) { return result; }
        const float mix = saturate(evalFloat(this, attrMix, tls, state));
        if (isZero(mix)) { return result; }

        if (state.isProvided(mIspc.mPresenceSet)) {
            float t = mix;
            if (state.isProvided(mIspc.mPresenceSetBlend)) {
                t *= state.getAttribute(TypedAttributeKey<float>(mIspc.mPresenceSetBlend));
            }

            const float attr = state.getAttribute(TypedAttributeKey<float>(mIspc.mPresenceSet));
            result = lerp(result, attr, t);
        }

        if (state.isProvided(mIspc.mPresenceMult)) {
            const float m = state.getAttribute(TypedAttributeKey<float>(mIspc.mPresenceMult));
            result = lerp(result, result * m, mix);
        }
    }
    return result;
}

float
DwaAdjustMaterial::resolveRefractiveIndex(TLState *tls,
                                          const State& state) const
{
    float result = 1.f;
    if (mInputMtl) {
        result = mInputMtl->resolveRefractiveIndex(tls, state);
    }
    return result;
}

bool
DwaAdjustMaterial::resolvePreventLightCulling(const State& state) const
{
    bool result = false;
    if (mInputMtl) {
        result = mInputMtl->resolvePreventLightCulling(state);
    }
    return result;
}

Vec3f
DwaAdjustMaterial::resolveSubsurfaceNormal(TLState *tls,
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
clampTo0(float& f)
{
    f = max(0.f, f);
}

static void
clampTo1(Color& c)
{
    c.r = min(1.f, c.r);
    c.g = min(1.f, c.g);
    c.b = min(1.f, c.b);
}

static void 
clampTo1(float& f)
{
    f = min(1.f, f);
}

// Most attributes follow this pattern. 
// NAME is capitalized attr name, eg Roughness
#define ADJUST_BASE(TYPE, NAME, ELEMS, CLAMP)           \
    if (state.isProvided(mIspc.m##NAME##Set)) {         \
        float t = mix;                                  \
        if (state.isProvided(mIspc.m##NAME##SetBlend)) {\
            t *= state.getAttribute(TypedAttributeKey<float>(mIspc.m##NAME##SetBlend));\
        }                                               \
        const TYPE attr = state.getAttribute(TypedAttributeKey<TYPE>(mIspc.m##NAME##Set));\
                                                        \
        for (uint8_t i = 0; i < ELEMS; ++i) {           \
            TYPE& x = *a##NAME[i];                      \
            x = lerp(x, attr, t);                       \
            if (CLAMP) {                                \
                clampTo0(x);                            \
                clampTo1(x);                            \
            }                                           \
        }                                               \
    }                                                   \
                                                        \
    if (state.isProvided(mIspc.m##NAME##Mult)) {        \
        const TYPE m = state.getAttribute(TypedAttributeKey<TYPE>(mIspc.m##NAME##Mult));\
                                                        \
        for (uint8_t i = 0; i < ELEMS; ++i) {           \
            TYPE& x = *a##NAME[i];                      \
            x = lerp(x, m * x, mix);                    \
            if (CLAMP) {                                \
                clampTo0(x);                            \
                clampTo1(x);                            \
            }                                           \
        }                                               \
    }

void
DwaAdjustMaterial::modifyParameters(TLState *tls,
                                    const State &state,
                                    ispc::DwaBaseParameters &params) const
{
    if (!get(attrOn)) { return; }

    // mask. emission might ignore this, used for early return after emission
    const float mix = saturate(evalFloat(this, attrMix, tls, state));

    const int emissionMode = get(attrEmissionMode);
    Color emission = evalColor(this, attrEmission, tls, state);

    float eMix;
    switch (emissionMode) {
    case ispc::AdjustEmissionMode::EMISSION_UNMASKED :
        eMix = 1.f;
        break;
    case ispc::AdjustEmissionMode::EMISSION_MASKED :
        eMix = mix;
        break;
    default:
        eMix = 0.f;
        break;
    }
    Color &e = asCpp(params.mEmission);
    e = e + eMix * emission;
    clampTo0(e);

    if (get(attrDisableClearcoat)) {
        params.mOuterSpecular = 0.f;
    }

    if (get(attrDisableSpecular)) {
        params.mSpecular = 0.f;
        params.mOuterSpecular = 0.f;
        params.mFabricSpecular = 0.f;
    }

    if (get(attrDisableDiffuse)) {
        asCpp(params.mAlbedo) = sBlack;
        asCpp(params.mWarpColor) = sBlack;
        asCpp(params.mWeftColor) = sBlack;
    }

    if (isZero(mix)) { return; }

    #define SPECULAR_ELEMS 3
    float * aSpecular[SPECULAR_ELEMS] = {
        &params.mSpecular,
        &params.mOuterSpecular,
        &params.mFabricSpecular
    };
    ADJUST_BASE(float, Specular, SPECULAR_ELEMS, true)

    #define ROUGHNESS_ELEMS 3
    float * aRoughness[ROUGHNESS_ELEMS] = {
        &params.mRoughness,
        &params.mOuterSpecularRoughness,
        &params.mFuzzRoughness
    };
    ADJUST_BASE(float, Roughness, ROUGHNESS_ELEMS, true)

    if (state.isProvided(mIspc.mRoughnessRemapInMin) && state.isProvided(mIspc.mRoughnessRemapInMax) &&
        state.isProvided(mIspc.mRoughnessRemapOutMin) && state.isProvided(mIspc.mRoughnessRemapOutMax)) {

        float inMin = state.getAttribute(TypedAttributeKey<float>(mIspc.mRoughnessRemapInMin));
        float inMax = state.getAttribute(TypedAttributeKey<float>(mIspc.mRoughnessRemapInMax));
        float outMin = state.getAttribute(TypedAttributeKey<float>(mIspc.mRoughnessRemapOutMin));
        float outMax = state.getAttribute(TypedAttributeKey<float>(mIspc.mRoughnessRemapOutMax));

        for (uint8_t i = 0; i < ROUGHNESS_ELEMS; ++i) {
            float& x = *aRoughness[i];
            const float original = x;
            x = (x - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
            x = (1.f - mix) * original + mix * x;
            clampTo0(x);
            clampTo1(x);
        }  
    }

    #define HSV_ELEMS 8
    Color * hsvColors[HSV_ELEMS] = {
        asCpp(&params.mFuzzAlbedo),
        asCpp(&params.mMetallicColor),
        asCpp(&params.mMetallicEdgeColor),
        asCpp(&params.mWarpColor),
        asCpp(&params.mWeftColor),
        asCpp(&params.mTransmissionColor),
        asCpp(&params.mAlbedo),
        asCpp(&params.mDiffuseTransmission)
    };

    float h = state.isProvided(mIspc.mColorHueShift) ? 
                  state.getAttribute(TypedAttributeKey<float>(mIspc.mColorHueShift)) : 0.f;
    float s = state.isProvided(mIspc.mColorSaturation) ? 
                  state.getAttribute(TypedAttributeKey<float>(mIspc.mColorSaturation)) : 1.f;
    float g = state.isProvided(mIspc.mColorGain) ? 
                  state.getAttribute(TypedAttributeKey<float>(mIspc.mColorGain)) : 1.f;
    Color gRGB = state.isProvided(mIspc.mColorGainRGB) ?
                  state.getAttribute(TypedAttributeKey<Color>(mIspc.mColorGainRGB)) : sWhite;
    for (uint8_t i = 0; i < HSV_ELEMS; ++i) {
        Color& c = *hsvColors[i];
        const Color original = c;
        if (!isZero(h)) {
            applyHueShift(h, c);
        }

        if (!isOne(s)) {
            applySaturation(s, c);
        }

        c = c * g * gRGB;

        c = lerp(original, c, mix);
        clampTo0(c);
        clampTo1(c);
    }
}

void
DwaAdjustMaterial::shade(const Material* self,
                         TLState *tls,
                         const State& state,
                         BsdfBuilder& bsdfBuilder)
{
    const DwaAdjustMaterial* me = static_cast<const DwaAdjustMaterial*>(self);
    const ispc::DwaAdjustMaterial* ispc = me->getISPCMaterialStruct();
    const bool castsCaustics = me->getCastsCaustics();

    ispc::DwaBaseParameters params;

    if (me->resolveParameters(tls, state, castsCaustics, params)) {
        me->createLobes(me, tls, state, bsdfBuilder, params, ispc->mUParams, sLabels);
    }
}

Vec3f
DwaAdjustMaterial::evalSubsurfaceNormal(const Material* self,
                                        TLState *tls,
                                        const State& state)
{
    const DwaAdjustMaterial* me = static_cast<const DwaAdjustMaterial*>(self);
    return me->resolveSubsurfaceNormal(tls, state);
}



// This is a construct to access the ISPC Data Struct from Inside the Scalar Material object.
// Note we are not using MATERIAL_GET_ISPC_CPTR to retrieve the ispc structure from the Material
// class as in other materials since that macro assumes a certain data layout we do not adhere to in
// DwaBase materials.
extern "C" {
const void*
getDwaAdjustMaterialStruct(const void* ptr)
{
    const DwaAdjustMaterial* classPtr = static_cast<const DwaAdjustMaterial*>(ptr);
    return (static_cast<const void*>(classPtr->getISPCMaterialStruct()));
}
}

//---------------------------------------------------------------------------


