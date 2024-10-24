// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file UVTransformMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>


struct StaticUVTransformMapData {
    // shade time error events - these can be static as long as we
    // ensure they have the same value across all instances.
    // no conditional registration of log events is allowed.
    uniform int sErrorMissingReferenceData;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(StaticUVTransformMapData);

struct UVTransformMap
{
    const Xform* mXform;
    uniform int mRefPKey;
    uniform Xform3f mUVRotationXform;

    uniform Color mFatalColor;
    uniform StaticUVTransformMapData* uniform mUVTransformMapDataPtr;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(UVTransformMap);

static void
rotateTexCoords(const uniform UVTransformMap * uniform me, varying Vec3f& pos)
{
    pos = transformPoint(me->mUVRotationXform, pos);
}

static Color
sample(const uniform Map *            uniform  map,
             uniform ShadingTLState * uniform  tls,
       const varying State &                   state)
{
    const uniform UVTransformMap * uniform me = MAP_GET_ISPC_CPTR(UVTransformMap, map);

    const varying Vec2f offset = evalAttrOffset(map, tls, state);
    const varying Vec2f scale = evalAttrScale(map, tls, state);

    const uniform Vec2f rotationCenter = getAttrRotationCenter(map);
    const uniform Vec3f rotationAxis = getAttrRotationAxis(map);

    // Get the position
    varying Vec3f pos;
    const uniform int space = getAttrSpace(map);
    if (space == SHADING_SPACE_TEXTURE) {
        pos = Vec3f_ctor(state.mSt.x,
                         state.mSt.y,
                         0.f);
    } else if (space == SHADING_SPACE_REFERENCE) {
        SHADER_DATA_getRefP(me->mRefPKey, tls, state, &pos);
    } else {
        pos = getP(state);

        if (space != SHADING_SPACE_RENDER) {
            pos = transformPoint(
                me->mXform,
                SHADING_SPACE_RENDER,
                space,
                state,
                pos
            );
        }
    }

    // Rotate coordinates.
    if (!isZero(getAttrRotationAngle(map))) {
        rotateTexCoords(me, pos);
    }
    // Scale and translate coords.
     pos.x = scale.x * pos.x + offset.x;
     pos.y = scale.y * pos.y + offset.y;
    return Color_ctor(pos.x, pos.y, 0.f);
}

DEFINE_MAP_SHADER(UVTransformMap, sample)

