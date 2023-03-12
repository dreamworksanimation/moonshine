// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

///
/// @file HairLayerMaterial.ispc
/// $Id$
///

#include "attributes.isph"
#include "labels.isph"

#include <moonray/rendering/shading/ispc/MaterialApi.isph>
#include <moonshine/material/dwabase/ispc/DwaBaseLayerable.isph>
#include <moonshine/material/dwabase/ispc/DwaBase.isph>
#include <moonshine/material/dwabase/ispc/Blending.isph>

struct HairLayerMaterialStaticData {
    // shade time error events - these can be static as long as we
    // ensure they have the same value across all instances.
    // no conditional registration of log events is allowed.
    uniform int sErrorMismatchedFresnelType;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(HairLayerMaterialStaticData);

struct HairLayerMaterial
{
    uniform SubMtlData mSubMtlA;
    uniform SubMtlData mSubMtlB;
    uniform BlendColorSpace mColorSpace;
    uniform TraceSet * uniform mSubsurfaceTraceSet;
    uniform intptr_t mEvalSubsurfaceNormal;
    uniform HairLayerMaterialStaticData * uniform mStaticData;
    uniform DwaBaseUniformParameters mUParams;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(HairLayerMaterial);

/// Accessor Function to retrieve the ISPC data pointer struct from C++ code branch
extern "C" const void* uniform getHairLayerMaterialStruct(const uniform Material* uniform);

/*
 * From DwaBaseLayerable.isph
 * #define DWABASELAYERABLE_RESOLVE_SUBSURFACE_FUNC_PARAMS      \
    const uniform Material* uniform me,                     \
    const varying State& state
 */
extern uniform int
HAIRLAYER_resolveSubsurfaceType(DWABASELAYERABLE_RESOLVE_SUBSURFACE_FUNC_ARGS)
{
    // Get Access to the ISPC Struct HairLayerMaterial
    const uniform HairLayerMaterial* uniform layerMaterial =
            (const uniform HairLayerMaterial* uniform)getHairLayerMaterialStruct(me);

    return layerMaterial->mUParams.mSubsurface;
}

/// Evaluates all the necessary attributes for DwaBase Materials
/*
 * From DwaBaseLayerable.isph
 * #define DWABASELAYERABLE_RESOLVE_PARAMS_FUNC_ARGS             \
    const uniform Material* uniform me,                     \
    uniform ShadingTLState *uniform tls,                    \
    const varying State& state,                             \
    const uniform bool castsCaustics,                       \
    varying DwaBaseParameters* uniform params
 */
extern bool
HAIRLAYER_resolveParameters(DWABASELAYERABLE_RESOLVE_PARAMS_FUNC_ARGS)
{
    // Get Access to the ISPC Struct HairLayerMaterial
    const uniform HairLayerMaterial* uniform layerMaterial =
            (const uniform HairLayerMaterial* uniform)getHairLayerMaterialStruct(me);

    // Get Access to Function Pointers to the Resolve() and CastsCaustics() Functions
    const uniform SubMtlData& subMtlA = layerMaterial->mSubMtlA;
    const uniform SubMtlData& subMtlB = layerMaterial->mSubMtlB;

    // LAYER MASK
    const varying float mask = saturate(evalAttrMask(me, tls, state));

    return DWABASE_blendHairParameters(me,
                                       tls,
                                       state,
                                       castsCaustics,
                                       params,
                                       &(layerMaterial->mUParams),
                                       layerMaterial->mColorSpace,
                                       layerMaterial->mEvalSubsurfaceNormal,
                                       subMtlB,
                                       subMtlA,
                                       layerMaterial->mStaticData->sErrorMismatchedFresnelType,
                                       mask);
}

extern uniform bool
HAIRLAYER_castsCaustics(const uniform Material *uniform me)
{
    const uniform HairLayerMaterial* uniform layerMaterial =
            (const uniform HairLayerMaterial* uniform)getHairLayerMaterialStruct(me);

    const uniform SubMtlData& subMtlA = layerMaterial->mSubMtlA;
    const uniform SubMtlData& subMtlB = layerMaterial->mSubMtlB;
    const uniform DWABASELAYERABLE_CastsCausticsFunc castsCausticsFnA = (const uniform DWABASELAYERABLE_CastsCausticsFunc) subMtlA.mGetCastsCausticsFunc;
    const uniform DWABASELAYERABLE_CastsCausticsFunc castsCausticsFnB = (const uniform DWABASELAYERABLE_CastsCausticsFunc) subMtlB.mGetCastsCausticsFunc;
    const uniform Material* uniform materialA =(const uniform Material * uniform) subMtlA.mDwaBaseLayerable;
    const uniform Material* uniform materialB =(const uniform Material * uniform) subMtlB.mDwaBaseLayerable;

    if (materialB && materialA) {
        return castsCausticsFnA(materialA) ||
               castsCausticsFnB(materialB);
    } else if (materialB) {
        return castsCausticsFnB(materialB);
    } else if (materialA){
        return castsCausticsFnA(materialA);
    } else {
        return false;
    }
}

extern float
HAIRLAYER_resolvePresence(const uniform Material* uniform me,
                          uniform ShadingTLState *uniform tls,
                          const varying State& state)
{
    // Get Access to the ISPC Struct HairLayerMaterial
    const uniform HairLayerMaterial* uniform layerMaterial =
            (const uniform HairLayerMaterial* uniform)getHairLayerMaterialStruct(me);

    const uniform SubMtlData& subMtlA = layerMaterial->mSubMtlA;
    const uniform SubMtlData& subMtlB = layerMaterial->mSubMtlB;
    const DWABASELAYERABLE_ResolvePresenceFunc presenceFnA = (DWABASELAYERABLE_ResolvePresenceFunc) subMtlA.mResolvePresenceFunc;
    const DWABASELAYERABLE_ResolvePresenceFunc presenceFnB = (DWABASELAYERABLE_ResolvePresenceFunc) subMtlB.mResolvePresenceFunc;
    const uniform Material* uniform materialA =(const uniform Material * uniform) subMtlA.mDwaBaseLayerable;
    const uniform Material* uniform materialB =(const uniform Material * uniform) subMtlB.mDwaBaseLayerable;

    // LAYER MASK
    const varying float mask = saturate(evalAttrMask(me, tls, state));

    // First verify that any submaterials that are connected are
    // DwaBaseLayerable, otherwise exit early
    if (!materialB) {
        // exit early, background material is missing or not Layerable
        return 1.f;
    }
    else if (!materialA || isZero(mask)) {
        return presenceFnB(materialB, tls, state);
    }
    else if (isOne(mask)) {
        return presenceFnA(materialA, tls, state);
    }
    
    const varying float presenceA = presenceFnA(materialA, tls, state);
    const varying float presenceB = presenceFnB(materialB, tls, state);

    return lerp(presenceB, presenceA, mask);
}

// TODO: This function is not tested and not completed because subsurface
// scattering currently goes through the scalar integrator. In particular, this
// might not properly handle the "enable sss input normal" toggle.
extern Vec3f
HAIRLAYER_resolveSubsurfaceNormal(const uniform Material* uniform me,
                                  uniform ShadingTLState *uniform tls,
                                  const varying State& state)
{
    // hair normal for SSS is derived procedurally, not blended
    const Vec3f T = normalize(getdPds(state));
    const Vec3f B = normalize(getdPdt(state));
    const Vec3f N = cross(B, T);
    return N;
}

// expose a way to retrieve a function pointer to the required
// ISPC functions from c++
export void *uniform HairLayerMaterial_getCastsCausticsFunc()
{
    return HAIRLAYER_castsCaustics;
}

export void *uniform HairLayerMaterial_getResolveSubsurfaceTypeFunc()
{
    return HAIRLAYER_resolveSubsurfaceType;
}

export void *uniform HairLayerMaterial_getResolveParametersFunc()
{
    return HAIRLAYER_resolveParameters;
}

export void *uniform HairLayerMaterial_getResolvePresenceFunc()
{
    return HAIRLAYER_resolvePresence;
}

export void *uniform HairLayerMaterial_getresolveSubsurfaceNormalFunc()
{
    return HAIRLAYER_resolveSubsurfaceNormal;
}

static void
shade(const uniform Material *      uniform  me,
            uniform ShadingTLState *uniform  tls,
      const varying State                   &state,
            varying BsdfBuilder             &bsdfBuilder)
{
    const uniform HairLayerMaterial * uniform material =
                (const uniform HairLayerMaterial* uniform)getHairLayerMaterialStruct(me);

    const uniform bool castsCaustics = HAIRLAYER_castsCaustics(me);

    varying DwaBaseParameters params;
    if (HAIRLAYER_resolveParameters(me, tls, state, castsCaustics, &params)) {
        DWABASE_createLobes(me, tls, state, bsdfBuilder, &params, &material->mUParams);
    }
}

DEFINE_MATERIAL_SHADER(HairLayerMaterial, shade)

