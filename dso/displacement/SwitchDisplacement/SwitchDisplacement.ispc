// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file SwitchDisplacement.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

typedef uniform intptr_t (*uniform DisplaceFn) (    const uniform Displacement *    uniform displacement,
                                                    uniform ShadingTLState *        uniform tls,
                                                    uniform int                     numState,
                                                    const varying State *           const uniform state,
                                                    varying Vec3f *                 uniform out);

enum SwitchConstants {
    MAX_DISP = 64,
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(SwitchConstants);

struct SwitchDisplacement
{
    uniform intptr_t mDisplacement[64];
    uniform intptr_t mDisplacementFunc[64];
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(SwitchDisplacement);

export const uniform SwitchDisplacement * uniform
SwitchDisplacement_get(const uniform Displacement * uniform displacement)
{
    return DISPLACEMENT_GET_ISPC_CPTR(SwitchDisplacement, displacement);
}

static varying Vec3f
displace(const uniform Displacement *   uniform me,
               uniform ShadingTLState * uniform tls,
         const varying State &          state)
{
    const uniform SwitchDisplacement * uniform cd =
            DISPLACEMENT_GET_ISPC_PTR(SwitchDisplacement, me);

    varying Vec3f displace = Vec3f_ctor(0.0f, 0.0f, 0.0f);

    uniform int choice = getAttrChoice(me);
    // cycle through choices
    choice = choice % MAX_DISP;

    const uniform Displacement* uniform dsp =
            (const uniform Displacement* uniform)(cd->mDisplacement[choice]);

    if (dsp) {
        DisplaceFn displacefn = (DisplaceFn) cd->mDisplacementFunc[choice];
        if (displacefn != NULL) {
            displacefn(dsp, tls, 1, &state, &displace);
        }
    }

    return displace;
}

DEFINE_DISPLACEMENT_SHADER(SwitchDisplacement, displace)
               
