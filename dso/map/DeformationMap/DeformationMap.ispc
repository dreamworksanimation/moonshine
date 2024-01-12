// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file DeformationMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

enum OutputMode {
    RGB              = 0,
    DEFORMATION_S    = 1,
    DEFORMATION_T    = 2,
    DEFORMATION_AVG  = 3
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(OutputMode);

struct StaticDeformationMapData {
    // shade time warning/error events - these can be static as long as we
    // ensure they have the same value across all instances.
    // no conditional registration of log events is allowed.
    uniform int sErrorZeroDerivatives;
    uniform int sErrorZeroRefDerivatives;
    uniform int sWarningZeroDerivatives;
    uniform int sWarningZeroRefDerivatives;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(StaticDeformationMapData);

struct DeformationMap
{
    uniform int mRefPKey;
    uniform Color mFatalColor;
    uniform StaticDeformationMapData* uniform mDataPtr;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(DeformationMap);

static Color
sample(const uniform Map * uniform map,
       uniform ShadingTLState * uniform tls,
       const varying State &state)
{
    const uniform DeformationMap * uniform me = MAP_GET_ISPC_CPTR(DeformationMap, map);

    // Current Space Vectors
    const Vec3f curdPds = getdPds(state);
    const Vec3f curdPdt = getdPdt(state);

    Vec3f curZ = cross(curdPds, curdPdt);
    const float lengthCurZ = length(curZ);

    // Errors if either curdPds or curdPdt are zero vectors or if they are identical.
    if (isZero(lengthCurZ)) {
        if (getAttrUseWarningColor(map)) {
            logEvent(map, me->mDataPtr->sWarningZeroDerivatives);
            return evalAttrWarningColor(map, tls, state);
        } else {
            logEvent(map, me->mDataPtr->sErrorZeroDerivatives);
            return me->mFatalColor;
        }
    }

    curZ = curZ / lengthCurZ;
    const Vec3f curX = normalize(curdPds);
    const Vec3f curY = normalize(cross(curZ, curX));

    // Reference Space Vectors
    Vec3f refdPds, refdPdt;
    refdPds = getdVec3fAttributeds(tls, state, me->mRefPKey);
    refdPdt = getdVec3fAttributedt(tls, state, me->mRefPKey);

    Vec3f refZ = cross(refdPds, refdPdt);
    const float lengthRefZ = length(refZ);

    // Errors if either refdPds or refdPdt are zero vectors or if they are identical.
    if (isZero(lengthRefZ)) {
        if (getAttrUseWarningColor(map)) {
            logEvent(map, me->mDataPtr->sWarningZeroRefDerivatives);
            return evalAttrWarningColor(map, tls, state);
        } else {
            logEvent(map, me->mDataPtr->sErrorZeroRefDerivatives);
            return me->mFatalColor;
        }
    }

    refZ = refZ / lengthRefZ;
    const Vec3f refX = normalize(refdPds);
    const Vec3f refY = normalize(cross(refZ, refX));

    // Stretch/Compression Factors
    const float deformationS = abs(dot(curdPds, curX) /
                                   dot(refdPds, refX));
    const float deformationT = abs(dot(curdPdt, curY) /
                                   dot(refdPdt, refY));
    const float deformationAvg = length(cross(curdPds, curdPdt)) /
                                 length(cross(refdPds, refdPdt));

    Color result;
    switch (getAttrOutputMode(map)) {
    case RGB:
        result = Color_ctor(deformationS, deformationT, deformationAvg);
        break;
    case DEFORMATION_S:
        result = Color_ctor(deformationS, deformationS, deformationS);
        break;
    case DEFORMATION_T:
        result = Color_ctor(deformationT, deformationT, deformationT);
        break;
    default:
        result = Color_ctor(deformationAvg, deformationAvg, deformationAvg);
    }

    return result;
}
DEFINE_MAP_SHADER(DeformationMap, sample)

