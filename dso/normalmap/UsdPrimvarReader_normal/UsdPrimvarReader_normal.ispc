// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file UsdPrimvarReader_normal.ispc

#include "attributes.isph"

#include <moonray/map/primvar/ispc/Primvar.isph>
#include <moonray/rendering/shading/ispc/MapApi.isph>

static Vec3f
sampleNormal(const uniform NormalMap* uniform map,
             uniform ShadingTLState* uniform tls,
             const varying State& state)
{
    varying Vec3f sample;
    const uniform UsdPrimvarReader * uniform me = MAP_GET_ISPC_PTR(UsdPrimvarReader, map);

    const uniform int attributeMapType = me->mAttributeMapType;
    if (attributeMapType == PRIMVAR_MAP_TYPE_N) {
        sample = state.mN;
    } else if (attributeMapType == PRIMVAR_MAP_TYPE_NG) {
        sample = state.mNg;
    } else {
        if (isProvided(state, me->mPrimitiveAttributeIndex)) {
            varying Vec3f objectSpaceNormal = getVec3fAttribute(tls, state, me->mPrimitiveAttributeIndex);
            sample = transformNormal(me->mXform,
                                     SHADING_SPACE_OBJECT,
                                     SHADING_SPACE_RENDER,
                                     state,
                                     objectSpaceNormal);
        } else {
            // the primitive attribute is unavailable
            const Vec3f fallback = evalAttrFallback(map, tls, state);
            ReferenceFrame frame;
            ReferenceFrame_init(frame, getN(state), normalize(getdPds(state)));
            sample = localToGlobal(frame, fallback);
            if (getAttrWarnWhenUnavailable(map)) {
                logEvent(map, me->mMissingAttributeEvent);
            }
        }
    }

    return sample;
}

DEFINE_NORMALMAP_SHADER(UsdPrimvarReader_normal, sampleNormal)

