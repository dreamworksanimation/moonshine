// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

///
/// @file DwaMixMaterial.ispc
/// $Id$
///

#include "attributes.isph"
#include "labels.isph"

#include <moonshine/material/dwabase/ispc/DwaBase.isph>
#include <moonray/rendering/shading/ispc/MaterialApi.isph>
#include <moonshine/material/dwabase/ispc/Blending.isph>

#define DWA_MIX_FLOAT_PAD 0.001f

enum DwaMixConstants {
    DWA_MIX_MAX_MATERIALS = 64
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(DwaMixConstants);

enum MixInterpolation {
    DWA_MIX_LINEAR,
    DWA_MIX_HOLD,
    DWA_MIX_NEAREST,
    DWA_MIX_SMOOTH
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(MixInterpolation);

struct DwaMixMaterial
{
    uniform intptr_t mSubMaterials[64];
    uniform SubMtlData mSubMaterialData[64];
    uniform BlendColorSpace mColorSpace;
    uniform TraceSet * uniform mSubsurfaceTraceSet;
    uniform intptr_t mEvalSubsurfaceNormalFn;
    uniform DwaBaseUniformParameters mUParams;
    uniform DwaBase * uniform mDwaBase;
    uniform bool mCastsCaustics;
    uniform int mInputMultiplier;
    uniform float mMaxMixValue;
    uniform MixInterpolation mMixInterpolation;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(DwaMixMaterial);


/// Accessor Function to retrieve the ISPC data pointer struct from C++ code branch
extern "C" const void* uniform getDwaMixMaterialStruct(const uniform Material* uniform);

export uniform DwaBaseAttributeFuncs DwaMixMaterial_collectAttributeFuncs()
{
    // This function is called when constructing DwaBase in scalar but
    // the functions are never used.   It's equivalent on the scalar side
    // collectAttributeKeys() gets the uniform glitter attribute keys which
    // are used in update to construct a Glitter object.
    uniform DwaBaseAttributeFuncs fns;
    DWABASE_initDwaBaseAttributeFuncs(fns); // set all struct members to 0 (null)
    return fns;
}

varying float
interpolateMix(varying float mix, uniform MixInterpolation mode, uniform int inputMultiplier) {
    mix = mix * inputMultiplier;
    switch (mode) {
    case DWA_MIX_LINEAR:
        break;
    case DWA_MIX_HOLD:
        mix = floor(mix);
        break;
    case DWA_MIX_NEAREST:
        mix = floor(mix + 0.5f);
        break;
    case DWA_MIX_SMOOTH:
        mix = (sin((mix + 0.5f) * sTwoPi) + (mix + 0.5f) * sTwoPi) * sOneOverTwoPi - 0.5f;
        break;
    default:
        break;
    }
    // Create "flat" spot around each whole number to account for floating point
    // error. Particularly important to ensure we hit the last or first material
    // or if a specific whole float value is provided
    if (abs((int)(mix + 0.5f) - mix) < DWA_MIX_FLOAT_PAD) {
        mix = (int)(mix + 0.5f);
    }

    return mix;
}

/*
 * From DwaBaseLayerable.isph
 * #define DWABASELAYERABLE_RESOLVE_SUBSURFACE_FUNC_ARGS      \
    const uniform Material* uniform me,                     \
    const varying State& state
 */
extern uniform int
DWAMIX_resolveSubsurfaceType(DWABASELAYERABLE_RESOLVE_SUBSURFACE_FUNC_ARGS)
{
    // Get Access to the ISPC Struct DwaMixMaterial
    const uniform DwaMixMaterial* uniform mixMaterial =
            (const uniform DwaMixMaterial* uniform)getDwaMixMaterialStruct(me);

    return mixMaterial->mDwaBase->mUParams.mSubsurface;
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
DWAMIX_resolveParameters(DWABASELAYERABLE_RESOLVE_PARAMS_FUNC_ARGS)
{
    // Get Access to the ISPC Struct DwaMixMaterial
    const uniform DwaMixMaterial* uniform mixMaterial =
            (const uniform DwaMixMaterial* uniform)getDwaMixMaterialStruct(me);

    varying float mix = clamp(evalAttrMix(me, tls, state), 0.f, mixMaterial->mMaxMixValue);
    mix = interpolateMix(mix, mixMaterial->mMixInterpolation, mixMaterial->mInputMultiplier);

    bool success;
    foreach_unique(val in mix) {
        const uniform unsigned int t0 = (int)val;
        const uniform SubMtlData& subMtl0 = mixMaterial->mSubMaterialData[t0];
        const uniform SubMtlData& subMtl1 = mixMaterial->mSubMaterialData[t0 + 1];

        success = DWABASE_blendParameters(me,
                                          tls,
                                          state,
                                          castsCaustics,
                                          params,
                                          &(mixMaterial->mDwaBase->mUParams),
                                          mixMaterial->mColorSpace,
                                          mixMaterial->mDwaBase->mGlitterPointer,
                                          mixMaterial->mDwaBase->mGlitterUniformParameters,
                                          mixMaterial->mEvalSubsurfaceNormalFn,
                                          subMtl0,
                                          subMtl1,
                                          val - t0);
    }
    return success;
}

extern uniform bool
DWAMIX_castsCaustics(const uniform Material *uniform me)
{
    const uniform DwaMixMaterial* uniform mixMaterial =
                (const uniform DwaMixMaterial* uniform)getDwaMixMaterialStruct(me);
    return mixMaterial->mCastsCaustics;
}

extern bool 
DWAMIX_resolvePreventLightCulling(const uniform Material* uniform me,
                                  const varying State& state)
{
    const uniform DwaMixMaterial* uniform mixMaterial =
        (const uniform DwaMixMaterial* uniform)getDwaMixMaterialStruct(me);

    return mixMaterial->mDwaBase->mUParams.mPreventLightCulling;
}

extern float
DWAMIX_resolvePresence(const uniform Material* uniform me,
                       uniform ShadingTLState *uniform tls,
                       const varying State& state)
{
    // Get Access to the ISPC Struct DwaMixMaterial
    const uniform DwaMixMaterial* uniform mixMaterial =
            (const uniform DwaMixMaterial* uniform)getDwaMixMaterialStruct(me);

    varying float mix = clamp(evalAttrMix(me, tls, state), 0.f, mixMaterial->mMaxMixValue);
    mix = interpolateMix(mix, mixMaterial->mMixInterpolation, mixMaterial->mInputMultiplier);

    float presence;
    foreach_unique(val in mix) {
        const uniform unsigned int t0 = (int)val;
        const uniform SubMtlData& subMtl0 = mixMaterial->mSubMaterialData[t0];
        const uniform SubMtlData& subMtl1 = mixMaterial->mSubMaterialData[t0 + 1];

        presence = DWABASE_blendPresence(tls,
                                         state,
                                         subMtl0,
                                         subMtl1,
                                         val - t0);
    }
    return presence;
}

// TODO: This function is not tested and not completed because subsurface
// scattering currently goes through the scalar integrator. In particular, this
// might not properly handle the "enable sss input normal" toggle.
extern Vec3f
DWAMIX_resolveSubsurfaceNormal(const uniform Material* uniform me,
                               uniform ShadingTLState *uniform tls,
                               const varying State& state)
{
    // Get Access to the ISPC Struct DwaMixMaterial
    const uniform DwaMixMaterial* uniform mixMaterial =
            (const uniform DwaMixMaterial* uniform)getDwaMixMaterialStruct(me);

    varying float mix = clamp(evalAttrMix(me, tls, state), 0.f, mixMaterial->mMaxMixValue);
    mix = interpolateMix(mix, mixMaterial->mMixInterpolation, mixMaterial->mInputMultiplier);

    Vec3f subSurfaceNormal;
    foreach_unique(val in mix) {
        const uniform unsigned int t0 = (int)val;
        const uniform SubMtlData& subMtl0 = mixMaterial->mSubMaterialData[t0];
        const uniform SubMtlData& subMtl1 = mixMaterial->mSubMaterialData[t0 + 1];
        subSurfaceNormal = DWABASE_blendSubsurfaceNormal(tls,
                                                         state,
                                                         subMtl0,
                                                         subMtl1,
                                                         val - t0);
    }
    return subSurfaceNormal;
}

// expose a way to retrieve a function pointer to the required
// ISPC functions from c++
export void *uniform DwaMixMaterial_getCastsCausticsFunc()
{
    return DWAMIX_castsCaustics;
}

export void *uniform DwaMixMaterial_getResolveSubsurfaceTypeFunc()
{
    return DWAMIX_resolveSubsurfaceType;
}

export void *uniform DwaMixMaterial_getResolveParametersFunc()
{
    return DWAMIX_resolveParameters;
}

export void *uniform DwaMixMaterial_getResolvePresenceFunc()
{
    return DWAMIX_resolvePresence;
}

export void *uniform DwaMixMaterial_getResolvePreventLightCullingFunc()
{
    return DWAMIX_resolvePreventLightCulling;
}

export void *uniform DwaMixMaterial_getresolveSubsurfaceNormalFunc()
{
    return DWAMIX_resolveSubsurfaceNormal;
}

static void
shade(const uniform Material *      uniform  me,
            uniform ShadingTLState *uniform  tls,
      const varying State                   &state,
            varying BsdfBuilder             &bsdfBuilder)
{
    // Get Access to the ISPC Struct DwaMixMaterial
    const uniform DwaMixMaterial* uniform mixMaterial =
            (const uniform DwaMixMaterial* uniform)getDwaMixMaterialStruct(me);

    const uniform bool castsCaustics = DWAMIX_castsCaustics(me);

    varying DwaBaseParameters params;
    if (DWAMIX_resolveParameters(me, tls, state, castsCaustics, &params)) {
        // get sss trace set
        params.mSubsurfaceTraceSet = mixMaterial->mSubsurfaceTraceSet;
        DWABASE_createLobes(me, tls, state, bsdfBuilder, &params, &mixMaterial->mUParams);
    }
}

DEFINE_MATERIAL_SHADER(DwaMixMaterial, shade)

