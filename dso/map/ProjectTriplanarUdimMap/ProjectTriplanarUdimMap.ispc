// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.isph"

#include <moonray/map/primvar/ispc/Primvar.isph>
#include <moonshine/map/projection/ispc/Projection.isph>

#include <moonray/rendering/shading/ispc/MapApi.isph>

struct ProjectTriplanarUdimMap
{
    const uniform Xform * uniform mXform;
    uniform Xform3f xProj2DXform; // 2D transform applied to x projection
    uniform Xform3f yProj2DXform; // 2D transform applied to y projection
    uniform Xform3f zProj2DXform; // 2D transform applied to z projection
    uniform int mRefPKey;
    uniform bool mHasValidProjector;
    uniform int mRefNKey;

    uniform PROJECTION_StaticData * uniform mStaticData;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(ProjectTriplanarUdimMap);

// Maps a given u or v coordinate value to [0, 1) range
// Input value can be positive or negative
inline varying float
remapToZeroOne(const varying float value)
{
    float result = fmod(value, 1.0f); // Maps value to [-1, 0) or [0, 1)
    
    // If above mapping results in [-1, 0), map it to [0, 1)
    return (result < 0) ? result + 1.0f : result;
}

inline varying int
chooseAxis(const varying Vec3f& normal)
{
    varying int max = 0;
    const varying Vec3f absNormal = abs(normal);
    
    // Check which coordinate among x,y,z is greatest
    // to select the direction to project the texture
    if (absNormal.y > absNormal.x) max = 1;
    if (absNormal.z > absNormal.y && absNormal.z > absNormal.x) max = 2;
    
    // Return 0 for X, 1 for Y and 2 for Z axis projection
    return max;
}

inline varying Vec3f
computeUv(const varying Vec3f& pos, 
    const varying Vec3f& normal,
    const varying int axis, 
    uniform bool useCorrectUVs,
    const uniform Xform3f &xProj2DXform, 
    const uniform Xform3f &yProj2DXform, 
    const uniform Xform3f &zProj2DXform)
{
    varying Vec3f resultUv;
    resultUv.z = 0.f;
    
    // Set correct u,v coordinates based on the face that is picked for projection
    // Offset u to account for UDIM layout of the three textures
    // Points projected outside [0, 1) are mapped back into [0, 1) range
    // Add 0.5 to u and v to account for center at the origin
    switch(axis) {
    case 2: // Project Z (XY plane), UDIM: 1003
        resultUv.x = pos.x + 0.5f;
        resultUv.y = pos.y + 0.5f;
        if (useCorrectUVs) {
            resultUv.x = 1.0f - resultUv.x;
        }
        resultUv =  transformPoint(zProj2DXform, resultUv);
        resultUv.x = remapToZeroOne(resultUv.x);
        resultUv.y = remapToZeroOne(resultUv.y);

        resultUv.x += 2.0f - sEpsilon; // For UDIM
        break;
    case 1: // Project Y (XZ plane), UDIM 1002
        resultUv.x = pos.x + 0.5f;
        resultUv.y = pos.z + 0.5f;
        if (useCorrectUVs) {
            resultUv.y = 1.0f - resultUv.y;
        }
        resultUv =  transformPoint(yProj2DXform, resultUv);
        resultUv.x = remapToZeroOne(resultUv.x);
        resultUv.y = remapToZeroOne(resultUv.y);

        resultUv.x += 1.0f - sEpsilon; // For UDIM
        break;
    case 0: // Project X (YZ plane), UDIM 1001
        resultUv.x = pos.z + 0.5f;
        resultUv.y = pos.y + 0.5f;
        if (useCorrectUVs) {
            resultUv.x = 1.0f - resultUv.x;
        }
        resultUv =  transformPoint(xProj2DXform, resultUv);
        resultUv.x = remapToZeroOne(resultUv.x);
        resultUv.y = remapToZeroOne(resultUv.y);
        // No UDIM offset in this case
        break;
    }

    return resultUv;
}

static Color
sample(const uniform Map* uniform map, uniform ShadingTLState* uniform tls,
       const varying State& state)
{
    const uniform ProjectTriplanarUdimMap * uniform me = MAP_GET_ISPC_PTR(ProjectTriplanarUdimMap, map);

    Color result = Color_ctor(0.f, 0.f, 0.f);

    if (!me->mHasValidProjector) {
        logEvent(map, me->mStaticData->sErrorMissingProjector);
        return result;
    }

    // Retrieve position and normal and transform into
    // the object space of the projector
    uniform int inputSourceMode;
    if (getAttrUseReferenceSpace(map)) {
        inputSourceMode = INPUT_SOURCE_MODE_REF_P_REF_N;
    } else {
        inputSourceMode = INPUT_SOURCE_MODE_P_N;
    }

    varying Vec3f pos, pos_ddx, pos_ddy, pos_ddz, inputPosition;
    if (!PRIMVAR_getPosition(tls, state,
                             inputSourceMode,
                             inputPosition,
                             me->mXform,
                             SHADING_SPACE_OBJECT,
                             me->mRefPKey,
                             pos, pos_ddx, pos_ddy, pos_ddz)) {
        // Log missing ref_P data message
        logEvent(map, me->mStaticData->sErrorMissingRefP);
        return result;
    }

    varying Vec3f normal, inputNormal;
    if (!PRIMVAR_getNormal(tls, state,
                           inputSourceMode,
                           inputNormal,
                           me->mXform,
                           SHADING_SPACE_OBJECT,
                           me->mRefPKey,
                           me->mRefNKey,
                           normal)) {
        // Log missing ref_N data message
        logEvent(map, me->mStaticData->sErrorMissingRefN);
        return result;
    }

    // Choose which texture to project based on normal orientation
    const varying int axis = chooseAxis(normal);

    // TODO: compute uvw as Vec3f wrt. dx, dy, dz
    const varying Vec3f uvw = computeUv(pos, normal, axis, getAttrUseCorrectUv(map),
                                        me->xProj2DXform, me->yProj2DXform, me->zProj2DXform);

    return asAColor(uvw);
}

DEFINE_MAP_SHADER(ProjectTriplanarUdimMap, sample)

