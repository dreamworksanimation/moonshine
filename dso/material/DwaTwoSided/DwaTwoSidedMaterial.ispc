// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file DwaTwoSidedMaterial.ispc
/// $Id$
///

#include "attributes.isph"
#include "labels.isph"

#include <moonshine/material/dwabase/ispc/DwaBase.isph>
#include <moonshine/material/dwabase/ispc/Blending.isph>
#include <moonray/rendering/shading/ispc/MaterialApi.isph>

struct DwaTwoSidedMaterial
{
    uniform SubMtlData mFrontMaterial;
    uniform SubMtlData mBackMaterial;
    uniform TraceSet * uniform mSubsurfaceTraceSet;
    uniform intptr_t mEvalSubsurfaceNormal;
    uniform DwaBaseUniformParameters mUParams;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(DwaTwoSidedMaterial);

/// Accessor Function to retrieve the ISPC data pointer struct from C++ code branch
extern "C" const void* uniform getDwaTwoSidedMaterialStruct(const uniform Material* uniform);

export uniform DwaBaseAttributeFuncs DwaTwoSidedMaterial_collectAttributeFuncs()
{
    // This function is called when constructing DwaBase in scalar but
    // the functions are never used.   It's equivalent on the scalar side
    // collectAttributeKeys() gets the uniform glitter attribute keys which
    // are used in update to construct a Glitter object.
    uniform DwaBaseAttributeFuncs fns;
    DWABASE_initDwaBaseAttributeFuncs(fns); // set all struct members to 0 (null)
    return fns;
}

/*
 * From DwaBaseLayerable.isph
 * #define DWABASELAYERABLE_RESOLVE_SUBSURFACE_FUNC_ARGS      \
    const uniform Material* uniform me,                     \
    const varying State& state
 */
extern uniform int
DWATWOSIDED_resolveSubsurfaceType(DWABASELAYERABLE_RESOLVE_SUBSURFACE_FUNC_ARGS)
{
    const uniform DwaTwoSidedMaterial* uniform twoSidedMaterial =
            (const uniform DwaTwoSidedMaterial* uniform)getDwaTwoSidedMaterialStruct(me);

    return twoSidedMaterial->mUParams.mSubsurface;
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
DWATWOSIDED_resolveParameters(DWABASELAYERABLE_RESOLVE_PARAMS_FUNC_ARGS)
{
    const uniform DwaTwoSidedMaterial* uniform twoSidedMaterial =
            (const uniform DwaTwoSidedMaterial* uniform)getDwaTwoSidedMaterialStruct(me);

    const uniform SubMtlData& front = twoSidedMaterial->mFrontMaterial;
    const uniform SubMtlData& back = twoSidedMaterial->mBackMaterial;
    const uniform Material* uniform frontMaterial = (const uniform Material * uniform) front.mDwaBaseLayerable;
    const uniform Material* uniform backMaterial = (const uniform Material * uniform) back.mDwaBaseLayerable;

    bool result = false;
    if (isEntering(state) && frontMaterial) {
        const DWABASELAYERABLE_ResolveParametersFunc resolveFnFront = (DWABASELAYERABLE_ResolveParametersFunc) front.mResolveParametersFunc;
        const uniform DWABASELAYERABLE_CastsCausticsFunc castsCausticsFnFront = (const uniform DWABASELAYERABLE_CastsCausticsFunc) front.mGetCastsCausticsFunc;
        result = resolveFnFront(frontMaterial, tls, state, castsCausticsFnFront(frontMaterial), params);
    } else if (!isEntering(state) && backMaterial) {
        const DWABASELAYERABLE_ResolveParametersFunc resolveFnBack = (DWABASELAYERABLE_ResolveParametersFunc) back.mResolveParametersFunc;
        const uniform DWABASELAYERABLE_CastsCausticsFunc castsCausticsFnBack = (const uniform DWABASELAYERABLE_CastsCausticsFunc) back.mGetCastsCausticsFunc;
        result = resolveFnBack(backMaterial, tls, state, castsCausticsFnBack(backMaterial), params);
    }

    return result;
}

extern void
DWATWOSIDED_createLobes(const uniform Material * uniform me,
                        uniform ShadingTLState * uniform tls,
                        const varying State             &state,
                        varying BsdfBuilder             &bsdfBuilder,
                        const varying DwaBaseParameters * uniform params,
                        const uniform DwaBaseUniformParameters * uniform uParams)
{

    const uniform DwaTwoSidedMaterial* uniform twoSidedMaterial =
            (const uniform DwaTwoSidedMaterial* uniform)getDwaTwoSidedMaterialStruct(me);

    const uniform SubMtlData& front = twoSidedMaterial->mFrontMaterial;
    const uniform SubMtlData& back = twoSidedMaterial->mBackMaterial;
    const uniform Material* uniform frontMaterial = (const uniform Material * uniform) front.mDwaBaseLayerable;
    const uniform Material* uniform backMaterial = (const uniform Material * uniform) back.mDwaBaseLayerable;

    if (isEntering(state) && frontMaterial) {
        const DWABASELAYERABLE_CreateLobesFunc createLobesFn = (DWABASELAYERABLE_CreateLobesFunc)front.mCreateLobesFunc;
        createLobesFn(frontMaterial, tls, state, bsdfBuilder, params, uParams);
    } else if (!isEntering(state) && backMaterial) {
        const DWABASELAYERABLE_CreateLobesFunc createLobesFn = (DWABASELAYERABLE_CreateLobesFunc)back.mCreateLobesFunc;
        createLobesFn(backMaterial, tls, state, bsdfBuilder, params, uParams);
    }
}

extern uniform bool
DWATWOSIDED_castsCaustics(const uniform Material *uniform me)
{
    const uniform DwaTwoSidedMaterial* uniform twoSidedMaterial =
            (const uniform DwaTwoSidedMaterial* uniform)getDwaTwoSidedMaterialStruct(me);

    const uniform SubMtlData& front = twoSidedMaterial->mFrontMaterial;
    const uniform SubMtlData& back = twoSidedMaterial->mBackMaterial;
    const uniform Material* uniform frontMaterial = (const uniform Material * uniform) front.mDwaBaseLayerable;
    const uniform Material* uniform backMaterial = (const uniform Material * uniform) back.mDwaBaseLayerable;
    const uniform DWABASELAYERABLE_CastsCausticsFunc castsCausticsFnFront = (const uniform DWABASELAYERABLE_CastsCausticsFunc) front.mGetCastsCausticsFunc;
    const uniform DWABASELAYERABLE_CastsCausticsFunc castsCausticsFnBack = (const uniform DWABASELAYERABLE_CastsCausticsFunc) back.mGetCastsCausticsFunc;

    // since casts caustics is a uniform value we need a consistent way to decide on/off
    // as it is possible that some lanes are front with others back.  Even if all lanes
    // happen to be the same, we would introduce non-determinism by checking and choosing
    // only the front or back material's castsCaustics setting.  This simple 'or' is a
    // safe approach that guarantees deterministic results.
    if (!frontMaterial || !backMaterial) {
        return false;
    } else {
        return (castsCausticsFnFront(frontMaterial) || castsCausticsFnBack(backMaterial));
    }
}

extern float
DWATWOSIDED_resolvePresence(const uniform Material* uniform me,
                            uniform ShadingTLState *uniform tls,
                            const varying State& state)
{
    const uniform DwaTwoSidedMaterial* uniform twoSidedMaterial =
            (const uniform DwaTwoSidedMaterial* uniform)getDwaTwoSidedMaterialStruct(me);

    const uniform SubMtlData& front = twoSidedMaterial->mFrontMaterial;
    const uniform SubMtlData& back = twoSidedMaterial->mBackMaterial;
    const uniform Material* uniform frontMaterial = (const uniform Material * uniform) front.mDwaBaseLayerable;
    const uniform Material* uniform backMaterial = (const uniform Material * uniform) back.mDwaBaseLayerable;

    bool result = false;
    if (isEntering(state) && frontMaterial) {
        const DWABASELAYERABLE_ResolvePresenceFunc presenceFnFront = (DWABASELAYERABLE_ResolvePresenceFunc) front.mResolvePresenceFunc;
        result = presenceFnFront(frontMaterial, tls, state);
    } else if (!isEntering(state) && backMaterial) {
        const DWABASELAYERABLE_ResolvePresenceFunc presenceFnBack = (DWABASELAYERABLE_ResolvePresenceFunc) back.mResolvePresenceFunc;
        result = presenceFnBack(backMaterial, tls, state);
    }

    return result;
}

// TODO: This function is not tested and not completed because subsurface
// scattering currently goes through the scalar integrator. In particular, this
// might not properly handle the "enable sss input normal" toggle.
extern Vec3f
DWATWOSIDED_resolveSubsurfaceNormal(const uniform Material* uniform me,
                                    uniform ShadingTLState *uniform tls,
                                    const varying State& state)
{
    varying Vec3f result = Vec3f_ctor(0.f, 0.f, 1.f);

    const uniform DwaTwoSidedMaterial* uniform twoSidedMaterial =
            (const uniform DwaTwoSidedMaterial* uniform)getDwaTwoSidedMaterialStruct(me);

    const uniform SubMtlData& front = twoSidedMaterial->mFrontMaterial;
    const uniform SubMtlData& back = twoSidedMaterial->mBackMaterial;
    const uniform Material* uniform frontMaterial = (const uniform Material * uniform) front.mDwaBaseLayerable;
    const uniform Material* uniform backMaterial = (const uniform Material * uniform) back.mDwaBaseLayerable;

    if (isEntering(state) && frontMaterial) {
        const DWABASELAYERABLE_resolveSubsurfaceNormalFunc normalFnFront = (DWABASELAYERABLE_resolveSubsurfaceNormalFunc) front.mResolveSubsurfaceNormalFunc;
        result = normalFnFront(frontMaterial, tls, state);
    } else if (!isEntering(state) && backMaterial) {
        const DWABASELAYERABLE_resolveSubsurfaceNormalFunc normalFnBack = (DWABASELAYERABLE_resolveSubsurfaceNormalFunc) back.mResolveSubsurfaceNormalFunc;
        result = normalFnBack(backMaterial, tls, state);
    }

    return result;
}

// expose a way to retrieve a function pointer to the required
// ISPC functions from c++
export void *uniform DwaTwoSidedMaterial_getCastsCausticsFunc()
{
    return DWATWOSIDED_castsCaustics;
}

export void *uniform DwaTwoSidedMaterial_getResolveSubsurfaceTypeFunc()
{
    return DWATWOSIDED_resolveSubsurfaceType;
}

export void *uniform DwaTwoSidedMaterial_getResolveParametersFunc()
{
    return DWATWOSIDED_resolveParameters;
}

export void *uniform DwaTwoSidedMaterial_getResolvePresenceFunc()
{
    return DWATWOSIDED_resolvePresence;
}

export void *uniform DwaTwoSidedMaterial_getresolveSubsurfaceNormalFunc()
{
    return DWATWOSIDED_resolveSubsurfaceNormal;
}

export void *uniform DwaTwoSidedMaterial_getCreateLobesFunc()
{
    return DWATWOSIDED_createLobes;
}

static void
shade(const uniform Material *      uniform  me,
            uniform ShadingTLState *uniform  tls,
      const varying State                   &state,
            varying BsdfBuilder             &bsdfBuilder)
{
    const uniform DwaTwoSidedMaterial* uniform twoSidedMaterial =
        (const uniform DwaTwoSidedMaterial* uniform)getDwaTwoSidedMaterialStruct(me);

    varying DwaBaseParameters params;
    DWABASELAYERABLE_initColorCorrectParameters(&params);

    if (DWATWOSIDED_resolveParameters(me, tls, state, false, &params)) {
        // get sss trace set
        params.mSubsurfaceTraceSet = twoSidedMaterial->mSubsurfaceTraceSet;

        DWATWOSIDED_createLobes(me, tls, state, bsdfBuilder, &params, &twoSidedMaterial->mUParams);
    }
}

DEFINE_MATERIAL_SHADER(DwaTwoSidedMaterial, shade)

