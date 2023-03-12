// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

#include <scene_rdl2/common/math/ispc/ColorSpace.isph>
#include <moonray/rendering/shading/ispc/RampControl.isph>

enum RampSlopesSize {
    RAMPMAP_MAX_DATASIZE = 20
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(RampSlopesSize);

struct RampMap
{
    ColorRampControl* mRampControl;

    const Xform* mXform;
    uniform int mRefPKey;

    uniform Color mFatalColor;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(RampMap);

enum WrapType {
    WRAP = 0,
    CLAMP
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(WrapType);


// ----------------------------------------------------------------------------

// Manipulate uvs to produce a resulting wave pattern
// Input uvs are modified
static void
applyWave(const uniform Vec2f& uvWave, varying Vec2f& uv)
{
    if (uvWave.x > 0.f) {
        uv.x = uv.x + sin(2.f * sPi * uv.y) * uvWave.x;
    }
    if (uvWave.y > 0.f) {
        uv.y = uv.y + sin(2.f * sPi * uv.x) * uvWave.y;
    }
}

// Wrap or clamp the uv based on user input
static void
applyWrap(const uniform int wrap, varying Vec2f& uv)
{
    if (wrap == WRAP) {
        if (uv.x > 1.0f || uv.x < 0.0f) {
            uv.x = uv.x - floor(uv.x);
        }
        if (uv.y > 1.0f || uv.y < 0.0f) {
            uv.y = uv.y - floor(uv.y);
        }
    } else { // Clamp
        uv.x = saturate(uv.x);
        uv.y = saturate(uv.y);
    }
}

// ----------------------------------------------------------------------------
static varying Color
evaluateRamp(const varying Vec3f& pos,
             const uniform Map * uniform map,
             const uniform RampMap * uniform me,
             uniform ShadingTLState * uniform tls,
             const varying State& state)
{
    varying Color result = Color_ctor(0.f);
    varying Vec2f uv = Vec2f_ctor(pos.x, pos.y);

    // Apply uv repeat; no repeats happen with default value of (1.0, 1.0)
    uv = uv * getAttrUvRepeat(map);

    // Apply wave in u and v
    const uniform Vec2f uvWave = getAttrUvWave(map);
    applyWave(uvWave, uv);

    // Handle wrap types
    const uniform int wrap = getAttrWrapType(map);
    applyWrap(wrap, uv);

    // Evaluate ramp result based on type of ramp and interpolation
    const uniform RampInterpolator2DType rampType = (RampInterpolator2DType)getAttrRampType(map);
    result = ColorRampControl_eval2D(uv,
                                     rampType,
                                     evalAttrInput(map, tls, state),
                                     me->mRampControl);

    return result;
}

static Color
sample(const uniform Map* uniform map, uniform ShadingTLState* uniform tls,
       const varying State& state)
{
    const uniform RampMap * uniform me = MAP_GET_ISPC_CPTR(RampMap, map);

    varying Vec3f pos;

    if (getAttrTextureEnum(map) == 1) {
        // Use alternate UVs.
        Vec3f uvw = evalAttrInputTextureCoordinate(map, tls, state);
        pos = Vec3f_ctor(uvw.x, uvw.y, 0.f);
    } else {
        // Use state UVs.
        // Get the position
        const uniform int space = getAttrSpace(map);
        if (space == SHADING_SPACE_TEXTURE) {
            pos = Vec3f_ctor(
                state.mSt.x,
                state.mSt.y,
                0.0f
            );
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
    }

    return evaluateRamp(pos, map, me, tls, state);
}

DEFINE_MAP_SHADER(RampMap, sample)

