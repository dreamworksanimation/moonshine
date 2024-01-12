// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file HairColorCorrectMaterial.ispc
/// $Id$
///

#include "attributes.isph"
#include "labels.isph"

#include <moonshine/material/dwabase/ispc/DwaBase.isph>
#include <moonray/rendering/shading/ispc/ColorCorrect.isph>
#include <moonray/rendering/shading/ispc/MaterialApi.isph>
#include <moonshine/material/dwabase/ispc/Blending.isph>

struct HairColorCorrectMaterial
{
    uniform SubMtlData mSubMaterial;
    uniform DwaBaseUniformParameters mUParams;
};

ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(HairColorCorrectMaterial);

/// Accessor Function to retrieve the ISPC data pointer struct from C++ code branch
extern "C" const void* uniform getHairColorCorrectMaterialStruct(const uniform Material* uniform);

/*
 * From DwaBaseLayerable.isph
 * #define DWABASELAYERABLE_RESOLVE_SUBSURFACE_FUNC_ARGS      \
    const uniform Material* uniform me,                     \
    const varying State& state
 */
extern uniform int
HAIRCOLORCORRECT_resolveSubsurfaceType(DWABASELAYERABLE_RESOLVE_SUBSURFACE_FUNC_ARGS)
{
    const uniform HairColorCorrectMaterial* uniform ccMtl =
            (const uniform HairColorCorrectMaterial* uniform)getHairColorCorrectMaterialStruct(me);
    const uniform SubMtlData& subMtl = ccMtl->mSubMaterial;
    const uniform Material* uniform material = (const uniform Material * uniform) subMtl.mDwaBaseLayerable;
    // Get Access to Function Pointers to ResolveSubsurfaceType
    const uniform DWABASELAYERABLE_ResolveSubsurfaceTypeFunc
    resolveSubsurfaceFn = (DWABASELAYERABLE_ResolveSubsurfaceTypeFunc) subMtl.mResolveSubsurfaceTypeFunc;
    // Resolve BSSRDF Type
    uniform int type = resolveSubsurfaceFn(material, state);
    return type;
}

// TODO: This function is not tested and not completed because subsurface
// scattering currently goes through the scalar integrator. In particular, this
// might not properly handle the "enable sss input normal" toggle.
extern Vec3f
HAIRCOLORCORRECT_resolveSubsurfaceNormal(const uniform Material* uniform me,
                                        uniform ShadingTLState *uniform tls,
                                        const varying State& state)
{
    varying Vec3f result = Vec3f_ctor(0.f, 0.f, 1.f);

    const uniform HairColorCorrectMaterial* uniform ccMtl =
            (const uniform HairColorCorrectMaterial* uniform)getHairColorCorrectMaterialStruct(me);

    const uniform SubMtlData& subMtl = ccMtl->mSubMaterial;
    const uniform Material* uniform material = (const uniform Material * uniform) subMtl.mDwaBaseLayerable;
    if (material) {
        const DWABASELAYERABLE_resolveSubsurfaceNormalFunc
        normalFn = (DWABASELAYERABLE_resolveSubsurfaceNormalFunc) subMtl.mResolveSubsurfaceNormalFunc;
        result = normalFn(material, tls, state);
    }
}

extern uniform bool
HAIRCOLORCORRECT_castsCaustics(const uniform Material *uniform me)
{
    const uniform HairColorCorrectMaterial* uniform ccMtl =
            (const uniform HairColorCorrectMaterial* uniform)getHairColorCorrectMaterialStruct(me);

    const uniform SubMtlData& subMtl = ccMtl->mSubMaterial;
    const uniform Material* uniform material = (const uniform Material * uniform) subMtl.mDwaBaseLayerable;
    const uniform DWABASELAYERABLE_CastsCausticsFunc
    castsCausticsFn = (const uniform DWABASELAYERABLE_CastsCausticsFunc) subMtl.mGetCastsCausticsFunc;

    if (!material) {
        return false;
    } else {
        return (castsCausticsFn(material));
    }
}

extern float
HAIRCOLORCORRECT_resolvePresence(const uniform Material* uniform me,
                                uniform ShadingTLState *uniform tls,
                                const varying State& state)
{
    const uniform HairColorCorrectMaterial* uniform ccMtl =
            (const uniform HairColorCorrectMaterial* uniform)getHairColorCorrectMaterialStruct(me);

    const uniform SubMtlData& subMtl = ccMtl->mSubMaterial;
    const uniform Material* uniform material = (const uniform Material * uniform) subMtl.mDwaBaseLayerable;

    bool result = 1.0f;
    if (material) {
        const DWABASELAYERABLE_ResolvePresenceFunc
        presenceFn = (DWABASELAYERABLE_ResolvePresenceFunc) subMtl.mResolvePresenceFunc;
        result = presenceFn(material, tls, state);
    }

    return result;
}

export void *uniform HairColorCorrectMaterial_getResolveSubsurfaceTypeFunc()
{
    return HAIRCOLORCORRECT_resolveSubsurfaceType;
}

// expose a way to retrieve a function pointer to the required
// ISPC functions from c++
export void *uniform HairColorCorrectMaterial_getCastsCausticsFunc()
{
    return HAIRCOLORCORRECT_castsCaustics;
}

export void *uniform HairColorCorrectMaterial_getResolvePresenceFunc()
{
    return HAIRCOLORCORRECT_resolvePresence;
}

export void *uniform HairColorCorrectMaterial_getresolveSubsurfaceNormalFunc()
{
    return HAIRCOLORCORRECT_resolveSubsurfaceNormal;
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
applyColorCorrections(const varying float hueShift,
                      const varying float saturation,
                      const varying float gainValue,
                      const uniform bool tmiEnabled,
                      const uniform Color tmi,
                      varying Color &c)
{
    if (!isZero(hueShift)) {
        applyHueShift(hueShift, c);
    }

    if (!isOne(saturation)) {
        applySaturation(saturation, c.r, c.g, c.b);
    }

    c = c * gainValue;

    if (tmiEnabled) {
        applyTMI(tmi.r, tmi.g, tmi.b, c.r, c.g, c.b);
    }
}

static void
modifyParameters(const uniform Material * uniform me,
                 uniform ShadingTLState * uniform tls,
                 const varying State &state,
                 varying DwaBaseParameters &params)
{
    if (!getAttrOn(me)) { return; }

    const varying float mix = saturate(evalAttrMix(me, tls, state));
    if (isZero(mix)) { return; }

    const varying float hueShift = evalAttrHueShift(me, tls, state);
    const varying float saturation = max(0.f, evalAttrSaturation(me, tls, state));
    const varying float gainValue = evalAttrGain(me, tls, state);
    const uniform bool tmiEnabled = getAttrTMIEnabled(me);
    const uniform Color tmi = getAttrTMI(me);

    // We'll apply color correction to these particular params.
    // Use pointer array to allow for processing them in a loop.
    #define NUM_COLOR_ARRAY_ELEMS 3
    varying Color * uniform colors[NUM_COLOR_ARRAY_ELEMS] = {
        &params.mHairParameters.mHairColor,
        &params.mHairParameters.mHairDiffuseFrontColor,
        &params.mHairParameters.mHairDiffuseBackColor

    };

    // process refl/trans values
    for (uniform uint8_t i = 0; i < NUM_COLOR_ARRAY_ELEMS; ++i) {
        varying Color& c = *colors[i];
        const varying Color original = c;
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
        varying Color& c = params.mEmission;
        const varying Color original = c;
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
HAIRCOLORCORRECT_resolveParameters(DWABASELAYERABLE_RESOLVE_PARAMS_FUNC_ARGS)
{
    const uniform HairColorCorrectMaterial* uniform ccMtl =
            (const uniform HairColorCorrectMaterial* uniform)getHairColorCorrectMaterialStruct(me);
    const uniform SubMtlData& subMtl = ccMtl->mSubMaterial;
    const uniform Material* uniform material = (const uniform Material * uniform) subMtl.mDwaBaseLayerable;
    bool result = false;
    if (material) {
        const DWABASELAYERABLE_ResolveParametersFunc
        resolveFn = (DWABASELAYERABLE_ResolveParametersFunc) subMtl.mResolveParametersFunc;
        const uniform DWABASELAYERABLE_CastsCausticsFunc
        castsCausticsFn = (const uniform DWABASELAYERABLE_CastsCausticsFunc) subMtl.mGetCastsCausticsFunc;
        result = resolveFn(material, tls, state, castsCausticsFn(material), params);
        // color correct params
        modifyParameters(me, tls, state, *params);
    }
    return result;
}

export void *uniform HairColorCorrectMaterial_getResolveParametersFunc()
{
    return HAIRCOLORCORRECT_resolveParameters;
}

static void
shade(const uniform Material *      uniform  me,
            uniform ShadingTLState *uniform  tls,
      const varying State                   &state,
            varying BsdfBuilder             &bsdfBuilder)
{
    const uniform HairColorCorrectMaterial * uniform material =
                    (const uniform HairColorCorrectMaterial* uniform)getHairColorCorrectMaterialStruct(me);
    const uniform bool castsCaustics = HAIRCOLORCORRECT_castsCaustics(me);

    varying DwaBaseParameters params;

    if (HAIRCOLORCORRECT_resolveParameters(me, tls, state, castsCaustics, &params)) {
        DWABASE_createLobes(me, tls, state, bsdfBuilder, &params, &material->mUParams);
    }
}

DEFINE_MATERIAL_SHADER(HairColorCorrectMaterial, shade)

