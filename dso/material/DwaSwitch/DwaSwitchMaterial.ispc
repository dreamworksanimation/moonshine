// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file DwaSwitchMaterial.ispc
/// $Id$
///

#include "attributes.isph"
#include "labels.isph"

#include <moonshine/material/dwabase/ispc/DwaBase.isph>
#include <moonray/rendering/shading/ispc/MaterialApi.isph>
#include <moonshine/material/dwabase/ispc/Blending.isph>

enum DwaSwitchConstants {
    DWA_MAX_MATERIALS = 64,
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(DwaSwitchConstants);

struct DwaSwitchMaterial
{
    uniform SubMtlData mSubMaterial;
    uniform TraceSet * uniform mSubsurfaceTraceSet;
    uniform intptr_t mEvalSubsurfaceNormal;
    uniform DwaBaseUniformParameters mUParams;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(DwaSwitchMaterial);

/// Accessor Function to retrieve the ISPC data pointer struct from C++ code branch
extern "C" const void* uniform getDwaSwitchMaterialStruct(const uniform Material* uniform);

/*
 * From DwaBaseLayerable.isph
 * #define DWABASELAYERABLE_RESOLVE_SUBSURFACE_FUNC_ARGS      \
    const uniform Material* uniform me,                     \
    const varying State& state
 */
extern uniform int
DWASWITCH_resolveSubsurfaceType(DWABASELAYERABLE_RESOLVE_SUBSURFACE_FUNC_ARGS)
{
    const uniform DwaSwitchMaterial* uniform switchMaterial =
            (const uniform DwaSwitchMaterial* uniform)getDwaSwitchMaterialStruct(me);
    const uniform SubMtlData& subMtl = switchMaterial->mSubMaterial;
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
DWASWITCH_resolveParameters(DWABASELAYERABLE_RESOLVE_PARAMS_FUNC_ARGS)
{
    const uniform DwaSwitchMaterial* uniform switchMaterial =
            (const uniform DwaSwitchMaterial* uniform)getDwaSwitchMaterialStruct(me);
    const uniform SubMtlData& subMtl = switchMaterial->mSubMaterial;
    const uniform Material* uniform material = (const uniform Material * uniform) subMtl.mDwaBaseLayerable;
    bool result = false;
    if (material) {
        const DWABASELAYERABLE_ResolveParametersFunc
        resolveFn = (DWABASELAYERABLE_ResolveParametersFunc) subMtl.mResolveParametersFunc;
        const uniform DWABASELAYERABLE_CastsCausticsFunc
        castsCausticsFn = (const uniform DWABASELAYERABLE_CastsCausticsFunc) subMtl.mGetCastsCausticsFunc;
        result = resolveFn(material, tls, state, castsCausticsFn(material), params);
        // override this, to make sure *this* material's evalSubsurfaceNormal() func is called,
        // and not the child material's func.  See MOONRAY-3768
        params->mEvalSubsurfaceNormalFn = switchMaterial->mEvalSubsurfaceNormal;
    }
    return result;
}

extern uniform bool
DWASWITCH_castsCaustics(const uniform Material *uniform me)
{
    const uniform DwaSwitchMaterial* uniform switchMaterial =
            (const uniform DwaSwitchMaterial* uniform)getDwaSwitchMaterialStruct(me);

    const uniform SubMtlData& subMtl = switchMaterial->mSubMaterial;
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
DWASWITCH_resolvePresence(const uniform Material* uniform me,
                          uniform ShadingTLState *uniform tls,
                          const varying State& state)
{
    const uniform DwaSwitchMaterial* uniform switchMaterial =
            (const uniform DwaSwitchMaterial* uniform)getDwaSwitchMaterialStruct(me);

    const uniform SubMtlData& subMtl = switchMaterial->mSubMaterial;
    const uniform Material* uniform material = (const uniform Material * uniform) subMtl.mDwaBaseLayerable;

    bool result = false;
    if (material) {
        const DWABASELAYERABLE_ResolvePresenceFunc
        presenceFn = (DWABASELAYERABLE_ResolvePresenceFunc) subMtl.mResolvePresenceFunc;
        result = presenceFn(material, tls, state);
    }

    return result;
}

extern bool
DWASWITCH_resolvePreventLightCulling(const uniform Material* uniform me,
                                     const varying State& state)
{
    const uniform DwaSwitchMaterial* uniform switchMaterial =
            (const uniform DwaSwitchMaterial* uniform)getDwaSwitchMaterialStruct(me);

    const uniform SubMtlData& subMtl = switchMaterial->mSubMaterial;
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
DWASWITCH_resolveSubsurfaceNormal(const uniform Material* uniform me,
                                  uniform ShadingTLState *uniform tls,
                                  const varying State& state)
{
    varying Vec3f result = Vec3f_ctor(0.f, 0.f, 1.f);

    const uniform DwaSwitchMaterial* uniform switchMaterial =
            (const uniform DwaSwitchMaterial* uniform)getDwaSwitchMaterialStruct(me);

    const uniform SubMtlData& subMtl = switchMaterial->mSubMaterial;
    const uniform Material* uniform material = (const uniform Material * uniform) subMtl.mDwaBaseLayerable;
    if (material) {
        const DWABASELAYERABLE_resolveSubsurfaceNormalFunc
        normalFn= (DWABASELAYERABLE_resolveSubsurfaceNormalFunc) subMtl.mResolveSubsurfaceNormalFunc;
        result = normalFn(material, tls, state);
    }
}

// expose a way to retrieve a function pointer to the required
// ISPC functions from c++
export void *uniform DwaSwitchMaterial_getCastsCausticsFunc()
{
    return DWASWITCH_castsCaustics;
}

export void *uniform DwaSwitchMaterial_getResolveSubsurfaceTypeFunc()
{
    return DWASWITCH_resolveSubsurfaceType;
}

export void *uniform DwaSwitchMaterial_getResolveParametersFunc()
{
    return DWASWITCH_resolveParameters;
}

export void *uniform DwaSwitchMaterial_getResolvePresenceFunc()
{
    return DWASWITCH_resolvePresence;
}

export void *uniform DwaSwitchMaterial_getResolvePreventLightCullingFunc()
{
    return DWASWITCH_resolvePreventLightCulling;
}

export void *uniform DwaSwitchMaterial_getresolveSubsurfaceNormalFunc()
{
    return DWASWITCH_resolveSubsurfaceNormal;
}

static void
shade(const uniform Material *      uniform  me,
            uniform ShadingTLState *uniform  tls,
      const varying State                   &state,
            varying BsdfBuilder             &bsdfBuilder)
{
    const uniform DwaSwitchMaterial* uniform thisMaterial =
        (const uniform DwaSwitchMaterial* uniform)getDwaSwitchMaterialStruct(me);

    varying DwaBaseParameters params;
    if (DWASWITCH_resolveParameters(me, tls, state, false, &params)) {
        // get sss trace set
        params.mSubsurfaceTraceSet = thisMaterial->mSubsurfaceTraceSet;

        DWABASE_createLobes(me, tls, state, bsdfBuilder, &params, &thisMaterial->mUParams);
    }
}

DEFINE_MATERIAL_SHADER(DwaSwitchMaterial, shade)

