// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file DwaColorCorrectMaterial.ispc
/// $Id$
///

#include "attributes.isph"
#include "labels.isph"

#include <moonshine/material/dwabase/ispc/DwaBase.isph>

#include <moonray/rendering/shading/ispc/ColorCorrect.isph>
#include <moonray/rendering/shading/ispc/MaterialApi.isph>
#include <scene_rdl2/common/math/ispc/ColorSpace.isph>
#include <moonshine/material/dwabase/ispc/Blending.isph>

struct DwaColorCorrectMaterial
{
    uniform SubMtlData mSubMaterial;
    uniform DwaBaseUniformParameters mUParams;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(DwaColorCorrectMaterial);

/// Accessor Function to retrieve the ISPC data pointer struct from C++ code branch
extern "C" const void* uniform getDwaColorCorrectMaterialStruct(const uniform Material* uniform);

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
    #define NUM_COLOR_ARRAY_ELEMS 8
    varying Color * uniform colors[NUM_COLOR_ARRAY_ELEMS] = {
        &params.mFuzzAlbedo,
        &params.mMetallicColor,
        &params.mMetallicEdgeColor,
        &params.mWarpColor,
        &params.mWeftColor,
        &params.mTransmissionColor,
        &params.mAlbedo,
        &params.mDiffuseTransmission // 8 elements
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

    if (!isZero(params.mToonDiffuseParams.mToonDiffuse) &&
        params.mToonDiffuseParams.mModel == TOON_DIFFUSE_RAMP) {
        const int rampPts = params.mToonDiffuseParams.mRampNumPoints;
        for (int i = 0; i < rampPts; ++i) {
            Color c = params.mToonDiffuseParams.mRampColors[i];
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
            params.mToonDiffuseParams.mRampColors[i] = c;
        }
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
 * #define DWABASELAYERABLE_RESOLVE_SUBSURFACE_FUNC_ARGS      \
    const uniform Material* uniform me,                     \
    const varying State& state
 */
extern uniform int
DWACOLORCORRECT_resolveSubsurfaceType(DWABASELAYERABLE_RESOLVE_SUBSURFACE_FUNC_ARGS)
{
    const uniform DwaColorCorrectMaterial* uniform ccMtl =
            (const uniform DwaColorCorrectMaterial* uniform)getDwaColorCorrectMaterialStruct(me);
    const uniform SubMtlData& subMtl = ccMtl->mSubMaterial;
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
DWACOLORCORRECT_resolveParameters(DWABASELAYERABLE_RESOLVE_PARAMS_FUNC_ARGS)
{
    const uniform DwaColorCorrectMaterial* uniform ccMtl =
            (const uniform DwaColorCorrectMaterial* uniform)getDwaColorCorrectMaterialStruct(me);
    const uniform SubMtlData& subMtl = ccMtl->mSubMaterial;
    const uniform Material* uniform material = (const uniform Material * uniform) subMtl.mDwaBaseLayerable;
    bool result = false;
    if (material) {
        const DWABASELAYERABLE_ResolveParametersFunc
        resolveFn = (DWABASELAYERABLE_ResolveParametersFunc) subMtl.mResolveParametersFunc;
        const uniform DWABASELAYERABLE_CastsCausticsFunc
        castsCausticsFn = (const uniform DWABASELAYERABLE_CastsCausticsFunc) subMtl.mGetCastsCausticsFunc;
        result = resolveFn(material, tls, state, castsCausticsFn(material), params);
        modifyParameters(me, tls, state, *params);
    }
    return result;
}

extern uniform bool
DWACOLORCORRECT_castsCaustics(const uniform Material *uniform me)
{
    const uniform DwaColorCorrectMaterial* uniform ccMtl =
            (const uniform DwaColorCorrectMaterial* uniform)getDwaColorCorrectMaterialStruct(me);

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
DWACOLORCORRECT_resolvePresence(const uniform Material* uniform me,
                                uniform ShadingTLState *uniform tls,
                                const varying State& state)
{
    const uniform DwaColorCorrectMaterial* uniform ccMtl =
            (const uniform DwaColorCorrectMaterial* uniform)getDwaColorCorrectMaterialStruct(me);

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

extern bool
DWACOLORCORRECT_resolvePreventLightCulling(const uniform Material* uniform me,
                                           const varying State& state)
{
    const uniform DwaColorCorrectMaterial* uniform ccMtl =
            (const uniform DwaColorCorrectMaterial* uniform)getDwaColorCorrectMaterialStruct(me);

    const uniform SubMtlData& subMtl = ccMtl->mSubMaterial;
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
DWACOLORCORRECT_resolveSubsurfaceNormal(const uniform Material* uniform me,
                                        uniform ShadingTLState *uniform tls,
                                        const varying State& state)
{
    varying Vec3f result = Vec3f_ctor(0.f, 0.f, 1.f);

    const uniform DwaColorCorrectMaterial* uniform ccMtl =
            (const uniform DwaColorCorrectMaterial* uniform)getDwaColorCorrectMaterialStruct(me);

    const uniform SubMtlData& subMtl = ccMtl->mSubMaterial;
    const uniform Material* uniform material = (const uniform Material * uniform) subMtl.mDwaBaseLayerable;
    if (material) {
        const DWABASELAYERABLE_resolveSubsurfaceNormalFunc
        normalFn = (DWABASELAYERABLE_resolveSubsurfaceNormalFunc) subMtl.mResolveSubsurfaceNormalFunc;
        result = normalFn(material, tls, state);
    }
}

// expose a way to retrieve a function pointer to the required
// ISPC functions from c++
export void *uniform DwaColorCorrectMaterial_getCastsCausticsFunc()
{
    return DWACOLORCORRECT_castsCaustics;
}

export void *uniform DwaColorCorrectMaterial_getResolveSubsurfaceTypeFunc()
{
    return DWACOLORCORRECT_resolveSubsurfaceType;
}

export void *uniform DwaColorCorrectMaterial_getResolveParametersFunc()
{
    return DWACOLORCORRECT_resolveParameters;
}

export void *uniform DwaColorCorrectMaterial_getResolvePresenceFunc()
{
    return DWACOLORCORRECT_resolvePresence;
}

export void *uniform DwaColorCorrectMaterial_getResolvePreventLightCullingFunc()
{
    return DWACOLORCORRECT_resolvePreventLightCulling;
}

export void *uniform DwaColorCorrectMaterial_getresolveSubsurfaceNormalFunc()
{
    return DWACOLORCORRECT_resolveSubsurfaceNormal;
}

static void
shade(const uniform Material *      uniform  me,
            uniform ShadingTLState *uniform  tls,
      const varying State                   &state,
            varying BsdfBuilder             &bsdfBuilder)
{
    const uniform DwaColorCorrectMaterial * uniform material =
                    (const uniform DwaColorCorrectMaterial* uniform)getDwaColorCorrectMaterialStruct(me);
    const uniform bool castsCaustics = DWACOLORCORRECT_castsCaustics(me);

    varying DwaBaseParameters params;

    if (DWACOLORCORRECT_resolveParameters(me, tls, state, castsCaustics, &params)) {
        DWABASE_createLobes(me, tls, state, bsdfBuilder, &params, &material->mUParams);
    }
}

DEFINE_MATERIAL_SHADER(DwaColorCorrectMaterial, shade)

