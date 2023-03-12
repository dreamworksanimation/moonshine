// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file DwaEmissiveMaterial.ispc

#include "attributes.isph"
#include "labels.isph"

#include <moonshine/material/dwabase/ispc/DwaBase.isph>
#include <moonshine/material/dwabase/ispc/DwaBaseLayerable.isph>
#include <moonray/rendering/shading/ispc/MaterialApi.isph>

export uniform DwaBaseAttributeFuncs DwaEmissiveMaterial_collectAttributeFuncs()
{
    uniform DwaBaseAttributeFuncs fns;
    DWABASE_initDwaBaseAttributeFuncs(fns); // set all struct members to 0 (null)

    fns.mGetAttrShowEmission                = (uniform intptr_t) (void*) getAttrShowEmission;
    fns.mEvalAttrEmission                   = (uniform intptr_t) (void*) evalAttrEmission;
    fns.mEvalAttrPresence                   = (uniform intptr_t) (void*) evalAttrPresence;

    return fns;
}

static void
shade(const uniform Material *      uniform  me,
            uniform ShadingTLState *uniform  tls,
      const varying State                   &state,
            varying BsdfBuilder             &bsdfBuilder)
{
    const uniform DwaBase * uniform dwaBase =
                    (const uniform DwaBase* uniform)getDwaBaseMaterialStruct(me);

    varying DwaBaseParameters params;
    DWABASE_resolveParameters(me, tls, state, false, &params);
    DWABASE_createLobes(me, tls, state, bsdfBuilder, &params, &dwaBase->mUParams);
}
DEFINE_MATERIAL_SHADER(DwaEmissiveMaterial, shade)

