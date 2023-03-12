// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file DwaAdjustMaterial.ispc
/// $Id$
///

#include "attributes.isph"
#include "labels.isph"

#include <moonshine/material/dwabase/ispc/DwaBase.isph>

#include <moonray/rendering/shading/ispc/ColorCorrect.isph>
#include <moonray/rendering/shading/ispc/MaterialApi.isph>
#include <moonshine/material/dwabase/ispc/Blending.isph>

enum AdjustEmissionMode {
    EMISSION_OFF = 0,
    EMISSION_MASKED = 1,
    EMISSION_UNMASKED = 2
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(AdjustEmissionMode);

enum AdjustOverrideMode {
    OVERRIDE_KEEP = 0,
    OVERRIDE_TRUE = 1,
    OVERRIDE_FALSE = 2
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(AdjustOverrideMode);

struct DwaAdjustMaterial
{
    uniform SubMtlData mSubMaterial;
    uniform DwaBaseUniformParameters mUParams;

    // attribute keys for adjustments. basic pattern is set, blend for that set value, and mult
    // each attr will have the same string separated by underscores unless otherwise noted
    // e.g. mSpecularValueBlend is an attr with the key "specular_set_blend"
    uniform int mSpecularSet;
    uniform int mSpecularSetBlend;
    uniform int mSpecularMult;
    uniform int mRoughnessSet;
    uniform int mRoughnessSetBlend;
    uniform int mRoughnessMult;
    uniform int mPresenceSet;
    uniform int mPresenceSetBlend;
    uniform int mPresenceMult;

    uniform int mRoughnessRemapInMin;
    uniform int mRoughnessRemapInMax;
    uniform int mRoughnessRemapOutMin;
    uniform int mRoughnessRemapOutMax;

    uniform int mColorHueShift;
    uniform int mColorSaturation;
    uniform int mColorGain;
    uniform int mColorGainRGB;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(DwaAdjustMaterial);

/// Accessor Function to retrieve the ISPC data pointer struct from C++ code branch
extern "C" const void* uniform getDwaAdjustMaterialStruct(const uniform Material* uniform);

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
// Note that there is a typedef in modifyParameters() for capitalized Float TYPE
#define ADJUST_BASE(TYPE, NAME, ELEMS, CLAMP)            \
    if (isProvided(state, aMtl->m##NAME##Set)) {         \
        varying float t = mix;                           \
        if (isProvided(state, aMtl->m##NAME##SetBlend)) {\
            t *= getFloatAttribute(tls, state, aMtl->m##NAME##SetBlend);\
        }                                                \
        const varying TYPE attr = get##TYPE##Attribute(tls, state, aMtl->m##NAME##Set);\
                                                         \
        for (uniform uint8_t i = 0; i < ELEMS; ++i) {    \
            varying TYPE& x = *a##NAME[i];               \
            x = lerp(x, attr, t);                        \
            if (CLAMP) {                                 \
                clampTo0(x);                             \
                clampTo1(x);                             \
            }                                            \
        }                                                \
    }                                                    \
                                                         \
    if (isProvided(state, aMtl->m##NAME##Mult)) {        \
        const varying TYPE m = get##TYPE##Attribute(tls, state, aMtl->m##NAME##Mult);\
                                                         \
        for (uniform uint8_t i = 0; i < ELEMS; ++i) {    \
            varying TYPE& x = *a##NAME[i];               \
            x = (1.f - mix) * x + mix * (m * x);         \
            if (CLAMP) {                                 \
                clampTo0(x);                             \
                clampTo1(x);                             \
            }                                            \
        }                                                \
    }

static void
modifyParameters(const uniform Material * uniform me,
                 uniform ShadingTLState * uniform tls,
                 const varying State &state,
                 varying DwaBaseParameters &params)
{
    if (!getAttrOn(me)) { return; }
    const varying float mix = saturate(evalAttrMix(me, tls, state));

    const uniform DwaAdjustMaterial* uniform aMtl =
            (const uniform DwaAdjustMaterial* uniform)getDwaAdjustMaterialStruct(me);

    const uniform int emissionMode = getAttrEmissionMode(me);
    const varying Color emission = evalAttrEmission(me, tls, state);

    float eMix;
    switch(emissionMode) {
    case EMISSION_UNMASKED :
        eMix = 1.f;
        break;
    case EMISSION_MASKED :
        eMix = mix;
        break;
    default :
        eMix = 0.f;
        break;
    }
    varying Color &e = params.mEmission;
    e = e + eMix * emission;
    clampTo0(e);

    if (getAttrDisableClearcoat(me)) {
        params.mOuterSpecular = 0.f;
    }

    if (getAttrDisableSpecular(me)) {
        params.mSpecular = 0.f;
        params.mOuterSpecular = 0.f;
        params.mFabricSpecular = 0.f;
    }

    if (getAttrDisableDiffuse(me)) {
        params.mAlbedo = sBlack;
        params.mWarpColor = sBlack;
        params.mWeftColor = sBlack;
    }

    if (isZero(mix)) return;

    // required for the macros to incorporate Float into attr request functions
    typedef float Float;

    #define SPECULAR_ELEMS 3
    varying float * uniform aSpecular[SPECULAR_ELEMS] = {
        &params.mSpecular,
        &params.mOuterSpecular,
        &params.mFabricSpecular
    };
    ADJUST_BASE(Float, Specular, SPECULAR_ELEMS, true)

    #define ROUGHNESS_ELEMS 3
    varying float * uniform aRoughness[ROUGHNESS_ELEMS] = {
        &params.mRoughness,
        &params.mOuterSpecularRoughness,
        &params.mFuzzRoughness
    };
    ADJUST_BASE(Float, Roughness, ROUGHNESS_ELEMS, true)

    if (isProvided(state, aMtl->mRoughnessRemapInMin) && isProvided(state, aMtl->mRoughnessRemapInMax) &&
        isProvided(state, aMtl->mRoughnessRemapOutMin) && isProvided(state, aMtl->mRoughnessRemapOutMax)) {

        const varying float inMin = getFloatAttribute(tls, state, aMtl->mRoughnessRemapInMin);
        const varying float inMax = getFloatAttribute(tls, state, aMtl->mRoughnessRemapInMax);
        const varying float outMin = getFloatAttribute(tls, state, aMtl->mRoughnessRemapOutMin);
        const varying float outMax = getFloatAttribute(tls, state, aMtl->mRoughnessRemapOutMax);

        for (uniform uint8_t i = 0; i < ROUGHNESS_ELEMS; ++i) {
            varying float& x = *aRoughness[i];
            const varying float original = x;
            x = (x - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
            x = lerp(original, x, mix);
            clampTo0(x);
            clampTo1(x);
        }
    }

    #define HSV_ELEMS 8
    varying Color * uniform hsvColors[HSV_ELEMS] = {
        &params.mFuzzAlbedo,
        &params.mMetallicColor,
        &params.mMetallicEdgeColor,
        &params.mWarpColor,
        &params.mWeftColor,
        &params.mTransmissionColor,
        &params.mAlbedo,
        &params.mDiffuseTransmission
    };

    const varying float h = isProvided(state, aMtl->mColorHueShift) ? 
                                getFloatAttribute(tls, state, aMtl->mColorHueShift) : 0.f;
    const varying float s = isProvided(state, aMtl->mColorSaturation) ? 
                                getFloatAttribute(tls, state, aMtl->mColorSaturation) : 1.f;
    const varying float g = isProvided(state, aMtl->mColorGain) ? 
                                getFloatAttribute(tls, state, aMtl->mColorGain) : 1.f;
    const varying Color gRGB = isProvided(state, aMtl->mColorGainRGB) ? 
                                   getColorAttribute(tls, state, aMtl->mColorGainRGB) : sWhite;

    for (uniform uint8_t i = 0; i < HSV_ELEMS; ++i) {
        varying Color& c = *hsvColors[i];
        const varying Color original = c;

        if (!isZero(h)) {
            applyHueShift(h, c);
        }

        if (!isOne(s)) {
            applySaturation(s, c.r, c.g, c.b);
        }

        c = c * g * gRGB;

        c = lerp(original, c, mix);
        clampTo0(c);
        clampTo1(c);
    }
}

/*
 * From DwaBaseLayerable.isph
 * #define DWABASELAYERABLE_RESOLVE_SUBSURFACE_FUNC_ARGS      \
    const uniform Material* uniform me,                     \
    const varying State& state
 */
extern uniform int
DWAADJUST_resolveSubsurfaceType(DWABASELAYERABLE_RESOLVE_SUBSURFACE_FUNC_ARGS)
{
    const uniform DwaAdjustMaterial* uniform adjustMtl =
            (const uniform DwaAdjustMaterial* uniform)getDwaAdjustMaterialStruct(me);
    const uniform SubMtlData& subMtl = adjustMtl->mSubMaterial;
    const uniform Material* uniform material = (const uniform Material * uniform) subMtl.mDwaBaseLayerable;
    // Get Access to Function Pointers to ResolveSubsurfaceType
    const uniform DWABASELAYERABLE_ResolveSubsurfaceTypeFunc
    resolveSubsurfaceFn = (DWABASELAYERABLE_ResolveSubsurfaceTypeFunc) subMtl.mResolveSubsurfaceTypeFunc;
    // Resolve BSSRDF Type
    uniform int type = resolveSubsurfaceFn(material, state);
    return type;
}


/*
 * From DwaBaseLayerable.isph
 * #define DWABASELAYERABLE_RESOLVE_FUNC_ARGS             \
    const uniform Material* uniform me,                     \
    uniform ShadingTLState *uniform tls,                    \
    const varying State& state,                             \
    const uniform bool castsCaustics,                       \
    varying DwaBaseParameters* uniform params
 */
extern bool
DWAADJUST_resolveParameters(DWABASELAYERABLE_RESOLVE_PARAMS_FUNC_ARGS)
{
    const uniform DwaAdjustMaterial* uniform adjustMtl =
            (const uniform DwaAdjustMaterial* uniform)getDwaAdjustMaterialStruct(me);
    const uniform SubMtlData& subMtl = adjustMtl->mSubMaterial;
    const uniform Material* uniform material = (const uniform Material * uniform) subMtl.mDwaBaseLayerable;
    bool result = false;
    if (material) {
        const DWABASELAYERABLE_ResolveParametersFunc
        resolveFn = (DWABASELAYERABLE_ResolveParametersFunc) subMtl.mResolveParametersFunc;
        const uniform DWABASELAYERABLE_CastsCausticsFunc
        castsCausticsFn = (const uniform DWABASELAYERABLE_CastsCausticsFunc) subMtl.mGetCastsCausticsFunc;
        result = resolveFn(material, tls, state, castsCausticsFn(material), params);

        if (result) {
            modifyParameters(me, tls, state, *params);
        }
    }

    return result;
}

extern uniform bool
DWAADJUST_castsCaustics(const uniform Material *uniform me)
{
    const uniform DwaAdjustMaterial* uniform adjustMtl =
            (const uniform DwaAdjustMaterial* uniform)getDwaAdjustMaterialStruct(me);

    const uniform SubMtlData& subMtl = adjustMtl->mSubMaterial;
    const uniform Material* uniform material = (const uniform Material * uniform) subMtl.mDwaBaseLayerable;
    const uniform DWABASELAYERABLE_CastsCausticsFunc
    castsCausticsFn = (const uniform DWABASELAYERABLE_CastsCausticsFunc) subMtl.mGetCastsCausticsFunc;
    const uniform int causticsOverride = getAttrOverrideCastsCaustics(me);

    if (!material) {
        return false;
    } else {
        if (causticsOverride == OVERRIDE_KEEP) {
            return castsCausticsFn(material);
        } else {
            return causticsOverride == OVERRIDE_TRUE;
        }
    }
}

extern float
DWAADJUST_resolvePresence(const uniform Material* uniform me,
                                uniform ShadingTLState *uniform tls,
                                const varying State& state)
{
    const uniform DwaAdjustMaterial* uniform aMtl =
            (const uniform DwaAdjustMaterial* uniform) getDwaAdjustMaterialStruct(me);

    const uniform SubMtlData& subMtl = aMtl->mSubMaterial;
    const uniform Material* uniform material = (const uniform Material * uniform) subMtl.mDwaBaseLayerable;

    float result = 1.0f;
    if (material) {
        const DWABASELAYERABLE_ResolvePresenceFunc
        presenceFn = (DWABASELAYERABLE_ResolvePresenceFunc) subMtl.mResolvePresenceFunc;
        result = presenceFn(material, tls, state);

        if (!getAttrOn(me)) { return result; }
        const varying float mix = saturate(evalAttrMix(me, tls, state));
        if (isZero(mix)) { return result; }

        if (isProvided(state, aMtl->mPresenceSet)) {
            float t = mix;
            if (isProvided(state, aMtl->mPresenceSetBlend)) {
                t *= getFloatAttribute(tls, state, aMtl->mPresenceSetBlend);
            }
            const float attr = getFloatAttribute(tls, state, aMtl->mPresenceSet);
            result = (1.f - t) * result + t * attr;
        }
        if (isProvided(state, aMtl->mPresenceMult)) {
            float m = getFloatAttribute(tls, state, aMtl->mPresenceMult);
            result = (1.f - mix) * result + mix * (m * result);
        }
    }

    return result;
}

extern bool
DWAADJUST_resolvePreventLightCulling(const uniform Material* uniform me,
                                           const varying State& state)
{
    const uniform DwaAdjustMaterial* uniform adjustMtl =
            (const uniform DwaAdjustMaterial* uniform)getDwaAdjustMaterialStruct(me);

    const uniform SubMtlData& subMtl = adjustMtl->mSubMaterial;
    const uniform Material* uniform material = (const uniform Material * uniform) subMtl.mDwaBaseLayerable;

    bool result = false;
    if (material) {
        const DWABASELAYERABLE_ResolvePreventLightCullingFunc
        preventLightCullingFn = (DWABASELAYERABLE_ResolvePreventLightCullingFunc) subMtl.mResolvePreventLightCullingFunc;
        result = preventLightCullingFn(material, state);
    }

    return result;
}

// TODO: This function is not tested and not completed because subsurface
// scattering currently goes through the scalar integrator. In particular, this
// might not properly handle the "enable sss input normal" toggle.
extern Vec3f
DWAADJUST_resolveSubsurfaceNormal(const uniform Material* uniform me,
                                        uniform ShadingTLState *uniform tls,
                                        const varying State& state)
{
    varying Vec3f result = Vec3f_ctor(0.f, 0.f, 1.f);

    const uniform DwaAdjustMaterial* uniform adjustMtl =
            (const uniform DwaAdjustMaterial* uniform)getDwaAdjustMaterialStruct(me);

    const uniform SubMtlData& subMtl = adjustMtl->mSubMaterial;
    const uniform Material* uniform material = (const uniform Material * uniform) subMtl.mDwaBaseLayerable;
    if (material) {
        const DWABASELAYERABLE_resolveSubsurfaceNormalFunc
        normalFn = (DWABASELAYERABLE_resolveSubsurfaceNormalFunc) subMtl.mResolveSubsurfaceNormalFunc;
        result = normalFn(material, tls, state);
    }
}

// expose a way to retrieve a function pointer to the required
// ISPC functions from c++
export void *uniform DwaAdjustMaterial_getCastsCausticsFunc()
{
    return DWAADJUST_castsCaustics;
}

export void *uniform DwaAdjustMaterial_getResolveSubsurfaceTypeFunc()
{
    return DWAADJUST_resolveSubsurfaceType;
}

export void *uniform DwaAdjustMaterial_getResolveParametersFunc()
{
    return DWAADJUST_resolveParameters;
}

export void *uniform DwaAdjustMaterial_getResolvePresenceFunc()
{
    return DWAADJUST_resolvePresence;
}

export void *uniform DwaAdjustMaterial_getResolvePreventLightCullingFunc()
{
    return DWAADJUST_resolvePreventLightCulling;
}

export void *uniform DwaAdjustMaterial_getresolveSubsurfaceNormalFunc()
{
    return DWAADJUST_resolveSubsurfaceNormal;
}

static void
shade(const uniform Material *      uniform  me,
            uniform ShadingTLState *uniform  tls,
      const varying State                   &state,
            varying BsdfBuilder             &bsdfBuilder)
{
    const uniform DwaAdjustMaterial * uniform material =
                    (const uniform DwaAdjustMaterial* uniform)getDwaAdjustMaterialStruct(me);
    const uniform bool castsCaustics = DWAADJUST_castsCaustics(me);

    varying DwaBaseParameters params;

    if (DWAADJUST_resolveParameters(me, tls, state, castsCaustics, &params)) {
        DWABASE_createLobes(me, tls, state, bsdfBuilder, &params, &material->mUParams);
    }
}

DEFINE_MATERIAL_SHADER(DwaAdjustMaterial, shade)

