// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

///
/// @file DwaLayerMaterial.ispc
/// $Id$
///

#include "attributes.isph"
#include "labels.isph"

#include <moonray/rendering/shading/ispc/MaterialApi.isph>
#include <moonshine/material/dwabase/ispc/DwaBaseLayerable.isph>
#include <moonshine/material/dwabase/ispc/DwaBase.isph>
#include <moonshine/material/dwabase/ispc/Blending.isph>

struct DwaLayerMaterial
{
    uniform SubMtlData mSubMtlA;
    uniform SubMtlData mSubMtlB;
    uniform BlendColorSpace mColorSpace;
    uniform TraceSet * uniform mSubsurfaceTraceSet;
    uniform intptr_t mEvalSubsurfaceNormal;
    uniform DwaBase * uniform mDwaBase;
    uniform DwaBaseUniformParameters mUParams;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(DwaLayerMaterial);

/// Accessor Function to retrieve the ISPC data pointer struct from C++ code branch
extern "C" const void* uniform getDwaLayerMaterialStruct(const uniform Material* uniform);

export uniform DwaBaseAttributeFuncs DwaLayerMaterial_collectAttributeFuncs()
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
 * #define DWABASELAYERABLE_RESOLVE_SUBSURFACE_FUNC_ARGS    \
    const uniform Material* uniform me,                     \
    const varying State& state
 */
extern uniform int
DWALAYER_resolveSubsurfaceType(DWABASELAYERABLE_RESOLVE_SUBSURFACE_FUNC_ARGS)
{
    // Get Access to the ISPC Struct DwaLayerMaterial
    const uniform DwaLayerMaterial* uniform layerMaterial =
            (const uniform DwaLayerMaterial* uniform)getDwaLayerMaterialStruct(me);

    return layerMaterial->mDwaBase->mUParams.mSubsurface;
}

/// Evaluates all the necessary attributes for DwaBase Materials
/*
 * From DwaBaseLayerable.isph
 * #define DWABASELAYERABLE_RESOLVE_PARAMS_FUNC_ARGS    \
    const uniform Material* uniform me,                 \
    uniform ShadingTLState *uniform tls,                \
    const varying State& state,                         \
    const uniform bool castsCaustics,                   \
    varying DwaBaseParameters* uniform params

 */
extern bool
DWALAYER_resolveParameters(DWABASELAYERABLE_RESOLVE_PARAMS_FUNC_ARGS)
{
    // Get Access to the ISPC Struct DwaLayerMaterial
    const uniform DwaLayerMaterial* uniform layerMaterial =
            (const uniform DwaLayerMaterial* uniform)getDwaLayerMaterialStruct(me);

    // Get Access to Function Pointers to the Resolve() and CastsCaustics() Functions
    const uniform SubMtlData& subMtlA = layerMaterial->mSubMtlA;
    const uniform SubMtlData& subMtlB = layerMaterial->mSubMtlB;

    // LAYER MASK
    const varying float mask = saturate(evalAttrMask(me, tls, state));

    return DWABASE_blendParameters(me,
                                   tls,
                                   state,
                                   castsCaustics,
                                   params,
                                   &(layerMaterial->mDwaBase->mUParams),
                                   layerMaterial->mColorSpace,
                                   layerMaterial->mDwaBase->mGlitterPointer,
                                   layerMaterial->mDwaBase->mGlitterUniformParameters,
                                   layerMaterial->mEvalSubsurfaceNormal,
                                   subMtlB,
                                   subMtlA,
                                   mask);
}

extern uniform bool
DWALAYER_castsCaustics(const uniform Material *uniform me)
{
    const uniform DwaLayerMaterial* uniform layerMaterial =
            (const uniform DwaLayerMaterial* uniform)getDwaLayerMaterialStruct(me);

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
    }
    else {
        return false;
    }
}

extern bool 
DWALAYER_resolvePreventLightCulling(const uniform Material* uniform me,
                                    const varying State& state)
{
    const uniform DwaLayerMaterial* uniform layerMaterial =
        (const uniform DwaLayerMaterial* uniform)getDwaLayerMaterialStruct(me);

    return layerMaterial->mDwaBase->mUParams.mPreventLightCulling;
}


extern float
DWALAYER_resolvePresence(const uniform Material* uniform me,
                         uniform ShadingTLState *uniform tls,
                         const varying State& state)
{
    // Get Access to the ISPC Struct DwaLayerMaterial
    const uniform DwaLayerMaterial* uniform layerMaterial =
            (const uniform DwaLayerMaterial* uniform)getDwaLayerMaterialStruct(me);

    const uniform SubMtlData& subMtlA = layerMaterial->mSubMtlA;
    const uniform SubMtlData& subMtlB = layerMaterial->mSubMtlB;
    // LAYER MASK
    const varying float mask = saturate(evalAttrMask(me, tls, state));
    return DWABASE_blendPresence(tls,
                                 state,
                                 subMtlB,
                                 subMtlA,
                                 mask);
}

// TODO: This function is not tested and not completed because subsurface
// scattering currently goes through the scalar integrator. In particular, this
// might not properly handle the "enable sss input normal" toggle.
extern Vec3f
DWALAYER_resolveSubsurfaceNormal(const uniform Material* uniform me,
                                uniform ShadingTLState *uniform tls,
                                const varying State& state)
{
    // Get Access to the ISPC Struct DwaLayerMaterial
    const uniform DwaLayerMaterial* uniform layerMaterial =
            (const uniform DwaLayerMaterial* uniform)getDwaLayerMaterialStruct(me);

    const uniform SubMtlData& subMtlA = layerMaterial->mSubMtlA;
    const uniform SubMtlData& subMtlB = layerMaterial->mSubMtlB;
    // LAYER MASK
    const varying float mask = saturate(evalAttrMask(me, tls, state));
    return DWABASE_blendSubsurfaceNormal(tls,
                                         state,
                                         subMtlB,
                                         subMtlA,
                                         mask);
}

// expose a way to retrieve a function pointer to the required
// ISPC functions from c++
export void *uniform DwaLayerMaterial_getCastsCausticsFunc()
{
    return DWALAYER_castsCaustics;
}

export void *uniform DwaLayerMaterial_getResolveSubsurfaceTypeFunc()
{
    return DWALAYER_resolveSubsurfaceType;
}

export void *uniform DwaLayerMaterial_getResolveParametersFunc()
{
    return DWALAYER_resolveParameters;
}

export void *uniform DwaLayerMaterial_getResolvePresenceFunc()
{
    return DWALAYER_resolvePresence;
}

export void *uniform DwaLayerMaterial_getResolvePreventLightCullingFunc()
{
    return DWALAYER_resolvePreventLightCulling;
}

export void *uniform DwaLayerMaterial_getresolveSubsurfaceNormalFunc()
{
    return DWALAYER_resolveSubsurfaceNormal;
}

static void
shade(const uniform Material *      uniform  me,
            uniform ShadingTLState *uniform  tls,
      const varying State                   &state,
            varying BsdfBuilder             &bsdfBuilder)
{
    // Get Access to the ISPC Struct DwaLayerMaterial
    const uniform DwaLayerMaterial* uniform layerMaterial =
            (const uniform DwaLayerMaterial* uniform)getDwaLayerMaterialStruct(me);

    const uniform bool castsCaustics = DWALAYER_castsCaustics(me);

    varying DwaBaseParameters params;
    DWABASELAYERABLE_initColorCorrectParameters(&params);
    if (DWALAYER_resolveParameters(me, tls, state, castsCaustics, &params)) {
        // get sss trace set
        params.mSubsurfaceTraceSet = layerMaterial->mSubsurfaceTraceSet;
        DWABASE_createLobes(me, tls, state, bsdfBuilder, &params, &layerMaterial->mUParams);
    }
}

DEFINE_MATERIAL_SHADER(DwaLayerMaterial, shade)

