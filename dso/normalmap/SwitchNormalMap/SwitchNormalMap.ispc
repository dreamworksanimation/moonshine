// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file SwitchNormalMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

enum SwitchNormalConstants {
    SN_MAX_INPUTS = 64,
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(SwitchNormalConstants);

struct SwitchNormalMap
{
    uniform intptr_t mNormalMap[64];
    uniform intptr_t mSampleNormalFn[64];
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(SwitchNormalMap);

static varying Vec3f
sampleNormal(const uniform NormalMap *uniform me,
             uniform ShadingTLState *uniform tls,
             const varying State &state)
{
    const uniform SwitchNormalMap * uniform sn = NORMALMAP_GET_ISPC_PTR(SwitchNormalMap, me);
    varying Vec3f norm = state.mN;

    uniform int choice = max(0, (uniform int) getAttrChoice(me));
    choice = choice % SN_MAX_INPUTS;

    const uniform NormalMap* uniform nm = 
        (const uniform NormalMap* uniform) (sn->mNormalMap[choice]);

    if (nm) {
        SampleNormalPtr sampleNormalFn = (SampleNormalPtr) sn->mSampleNormalFn[choice];
        if (sampleNormalFn != NULL) {
            sampleNormalFn(nm, tls, &state, &norm);
        }
    }

    return norm;
}

DEFINE_NORMALMAP_SHADER(SwitchNormalMap, sampleNormal)

