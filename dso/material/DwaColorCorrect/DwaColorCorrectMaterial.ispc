// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file DwaColorCorrectMaterial.ispc
/// $Id$
///

#include "attributes.isph"
#include "labels.isph"

#include <moonshine/material/dwabase/ispc/DwaBase.isph>

#include <moonray/rendering/shading/ispc/MaterialApi.isph>
#include <scene_rdl2/common/math/ispc/ColorSpace.isph>
#include <moonshine/material/dwabase/ispc/Blending.isph>

struct DwaColorCorrectMaterial
{
    uniform SubMtlData mSubMaterial;
    uniform intptr_t mEvalSubsurfaceNormal;
    uniform DwaBaseUniformParameters mUParams;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(DwaColorCorrectMaterial);

/// Accessor Function to retrieve the ISPC data pointer struct from C++ code branch
extern "C" const void* uniform getDwaColorCorrectMaterialStruct(const uniform Material* uniform);

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
        const int cc = params->mNumColorCorrections;
        params->mColorCorrectParams[cc].mOn = getAttrOn(me);
        params->mColorCorrectParams[cc].mMix = saturate(evalAttrMix(me, tls, state));
        params->mColorCorrectParams[cc].mHueShift = evalAttrHueShift(me, tls, state);
        params->mColorCorrectParams[cc].mSaturation = max(0.f, evalAttrSaturation(me, tls, state));
        params->mColorCorrectParams[cc].mGain = evalAttrGain(me, tls, state);
        params->mColorCorrectParams[cc].mTmiEnabled = getAttrTMIEnabled(me);
        params->mColorCorrectParams[cc].mTmi = evalAttrTMI(me, tls, state);
        params->mNumColorCorrections = min(DWABASE_MAX_COLOR_CORRECTIONS,
                                           params->mNumColorCorrections + 1);

        const DWABASELAYERABLE_ResolveParametersFunc
        resolveFn = (DWABASELAYERABLE_ResolveParametersFunc) subMtl.mResolveParametersFunc;
        const uniform DWABASELAYERABLE_CastsCausticsFunc
        castsCausticsFn = (const uniform DWABASELAYERABLE_CastsCausticsFunc) subMtl.mGetCastsCausticsFunc;
        result = resolveFn(material, tls, state, castsCausticsFn(material), params);
        // override this, to make sure *this* material's evalSubsurfaceNormal() func is called,
        // and not the child material's func. Also see DwaSwitchMaterial.
        params->mEvalSubsurfaceNormalFn = ccMtl->mEvalSubsurfaceNormal;
    }
    return result;
}

extern void
DWACOLORCORRECT_createLobes(const uniform Material * uniform me,
                            uniform ShadingTLState * uniform tls,
                            const varying State             &state,
                            varying BsdfBuilder             &bsdfBuilder,
                            const varying DwaBaseParameters * uniform params,
                            const uniform DwaBaseUniformParameters * uniform uParams)
{
    const uniform DwaColorCorrectMaterial* uniform ccMtl =
            (const uniform DwaColorCorrectMaterial* uniform)getDwaColorCorrectMaterialStruct(me);

    const uniform SubMtlData& subMtl = ccMtl->mSubMaterial;
    const uniform Material* uniform material = (const uniform Material * uniform) subMtl.mDwaBaseLayerable;

    if (material) {
        const DWABASELAYERABLE_CreateLobesFunc
        createLobesFn = (DWABASELAYERABLE_CreateLobesFunc) subMtl.mCreateLobesFunc;
        createLobesFn(material, tls, state, bsdfBuilder, params, uParams);
    }
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

export void *uniform DwaColorCorrectMaterial_getCreateLobesFunc()
{
    return DWACOLORCORRECT_createLobes;
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
    DWABASELAYERABLE_initColorCorrectParameters(&params);

    if (DWACOLORCORRECT_resolveParameters(me, tls, state, castsCaustics, &params)) {
        DWACOLORCORRECT_createLobes(me, tls, state, bsdfBuilder, &params, &material->mUParams);
    }
}

DEFINE_MATERIAL_SHADER(DwaColorCorrectMaterial, shade)

