// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

// Reproduction in whole or in part without prior written worleyPermission of a

/// @file NoiseWorleyMap_v2.ispc

#include "attributes.isph"

#include <moonray/map/primvar/ispc/Primvar.isph>

#include <moonray/rendering/shading/ispc/MapApi.isph>

#include <moonshine/common/interpolation/ispc/Interpolation.isph>
#include <moonshine/common/noise/ispc/Noise.isph>
#include <moonshine/common/noise/ispc/Worley.isph>

struct StaticNoiseWorleyMapData {
    // shade time error events - these can be static as long as we
    // ensure they have the same value across all instances.
    // no conditional registration of log events is allowed.
    uniform Color sFatalColor;
    uniform int sErrorMissingReferenceData;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(StaticNoiseWorleyMapData);

enum NoiseWorleyMapConstants{
    NOISE_WORLEY_MAP_TABLE_SIZE = 2048
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(NoiseWorleyMapConstants);

enum NoiseWorleyMapOutputMode {
    DISTANCE = 0,
    GRADIENT = 1,
    CELLID = 2,
    CELL_EDGES = 3,
    POINTS = 4
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(NoiseWorleyMapOutputMode);

struct NoiseWorleyMap_v2
{
    const Xform *mXform;
    const NOISE_Worley *mNoise;
    uniform int mRefPKey;
    uniform int mHairSurfaceSTKey;
    uniform int mHairClosestSurfaceSTKey;
    uniform bool mUseStaticXform;
    uniform Xform3f mStaticXform;

    uniform StaticNoiseWorleyMapData* uniform mNoiseWorleyMapDataPtr;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(NoiseWorleyMap_v2);

static Color
adjust(float d,
       const Vec2f& remapVal,
       const float biasVal,
       const float gainVal,
       uniform bool useSmoothstep,
       const Vec2f& smoothstepVal,
       uniform bool invert,
       const Color& colorAVal,
       const Color& colorBVal)
{
    // Remap
    float denom = remapVal.y - remapVal.x;
    d = (d - remapVal.x) / ((denom != 0.f) ? denom : 0.000001f);

    // Bias/Gain
    d = gain(bias(d, biasVal), gainVal);

    // Smoothstep
    if (useSmoothstep) {
        d = INTERPOLATION_smoothStep(d, smoothstepVal.x, smoothstepVal.y);
    }

    Color noise;
    noise.r = colorAVal.r * (1.0f - d) + colorBVal.r * d;
    noise.g = colorAVal.g * (1.0f - d) + colorBVal.g * d;
    noise.b = colorAVal.b * (1.0f - d) + colorBVal.b * d;
    if(invert) {
        noise.r = 1.0f - noise.r;
        noise.g = 1.0f - noise.g;
        noise.b = 1.0f - noise.b;
    }
    return noise;
}

static Color
sample(const uniform Map * uniform map,
       uniform ShadingTLState *uniform tls,
       const State &state)
{
    const uniform NoiseWorleyMap_v2 * uniform me =
        MAP_GET_ISPC_CPTR(NoiseWorleyMap_v2, map);

    // Get the position
    varying Vec3f pos;
    uniform SHADING_Space space = (SHADING_Space)getAttrSpace(map);
    if (space == SHADING_SPACE_TEXTURE) {
        pos = Vec3f_ctor(state.mSt.x,
                         state.mSt.y,
                         0.0f);
    } else if (space == SHADING_SPACE_HAIR_SURFACE_ST) {
        Vec2f uv = getVec2fAttribute(tls, state, me->mHairSurfaceSTKey);
        pos = Vec3f_ctor(uv.x, uv.y, 0.0f);
    } else if (space == SHADING_SPACE_HAIR_CLOSEST_SURFACE_ST) {
        Vec2f uv = getVec2fAttribute(tls, state, me->mHairClosestSurfaceSTKey);
        pos = Vec3f_ctor(uv.x, uv.y, 0.0f);
    } else {
        uniform int inputSourceMode;
        varying Vec3f inputPosition;
        if (space == SHADING_SPACE_REFERENCE) {
            inputSourceMode = INPUT_SOURCE_MODE_REF_P_REF_N;
        } else if (space == SHADING_SPACE_INPUT_COORDINATES) {
            inputSourceMode = INPUT_SOURCE_MODE_ATTR;
            inputPosition = evalAttrInputTextureCoordinate(map, tls, state);
        } else {
            inputSourceMode = INPUT_SOURCE_MODE_P_N;
        }

        Vec3f pos_ddx, pos_ddy, pos_ddz;
        if (!PRIMVAR_getPosition(tls, state,
                                 inputSourceMode,
                                 inputPosition,
                                 me->mXform,
                                 space,
                                 me->mRefPKey,
                                 pos, pos_ddx, pos_ddy, pos_ddz)) {
            // Log missing ref_P data message
            logEvent(map, me->mNoiseWorleyMapDataPtr->sErrorMissingReferenceData);
            return me->mNoiseWorleyMapDataPtr->sFatalColor;
        }
    }

    // Scale P by frequency multiplier
    pos = pos * evalAttrFrequency(map, tls, state);

    // Further transform pos using the transform parameters
    if (me->mUseStaticXform) {
        pos = transformPoint(me->mStaticXform, pos);
    } 
    else {
        // mapped xforms - compose the xform for each sample
        Xform3f xform = NOISE_orderedCompose(
            evalAttrTranslation(map, tls, state),
            evalAttrRotation(map, tls, state) * sPi/180.f,
            evalAttrScale(map, tls, state),
            getAttrTransformationOrder(map), 
            getAttrRotationOrder(map)
        );
        pos = transformPoint(xform, pos);
    }

    // Worley ultimately degrades into a series of table lookups,
    // which defeats the function and operator overloading of autodiff.
    // we'll need to use finite differences to compute the value
    // and derivatives of this function.

    // For each bindable input parameter, we evaluate the value.
    float f0 = evalAttrF1(map, tls, state);
    float f1 = evalAttrF2(map, tls, state);
    float f2 = evalAttrF3(map, tls, state);
    float f3 = evalAttrF4(map, tls, state);
    // Clamp jitter to 0-1
    float jitter = clamp(evalAttrJitter(map, tls, state), 0.f, 1.f);
    float maxLevel = evalAttrMaxLevel(map, tls, state);
    float minkowskiNumber = evalAttrMinkowskiNumber(map, tls, state);
    float biasVal = evalAttrBias(map, tls, state);
    float gainVal = evalAttrGain(map, tls, state);
    Vec2f remap = evalAttrRemap(map, tls, state);
    Vec2f smoothstepVal = evalAttrSmoothstep(map, tls, state);
    Color colorA = evalAttrColorA(map, tls, state);
    Color colorB = evalAttrColorB(map, tls, state);

    // Distance method is not bindable so no evaluation
    const uniform int distanceMethod = getAttrDistanceMethod(map);

    Color noise;

    varying NOISE_WorleySample noiseSample;

    uniform Arena * uniform arena = tls->mArena;
    varying NOISE_WorleyPoint *uniform worleyPoints = (varying NOISE_WorleyPoint *uniform)
        Arena_allocArray(arena, NOISE_WORLEY_MAX_SEARCH_POINTS, sizeof(varying NOISE_WorleyPoint));

    noise = Color_ctor(0.0f, 0.0f, 0.0f);

    noiseSample.position = pos;
    noiseSample.radius = 0.0f;
    NOISE_worleySearchPointsFractal(me->mNoise, tls, jitter, minkowskiNumber,
                                    maxLevel, noiseSample, worleyPoints);

    switch(getAttrOutputMode(map)) {
        case DISTANCE:
            {
                float d = f0 * worleyPoints[0].dist +
                          f1 * worleyPoints[1].dist + 
                          f2 * worleyPoints[2].dist + 
                          f3 * worleyPoints[3].dist; 
                noise = adjust(d, remap, biasVal, gainVal,
                               getAttrUseSmoothstep(map), smoothstepVal,
                               getAttrInvert(map), colorA, colorB);
            }
            break;
        case GRADIENT:
            noise.r =    f0 * worleyPoints[0].gradient.x + 
                         f1 * worleyPoints[1].gradient.x + 
                         f2 * worleyPoints[2].gradient.x + 
                         f3 * worleyPoints[3].gradient.x;
            noise.g =    f0 * worleyPoints[0].gradient.y + 
                         f1 * worleyPoints[1].gradient.y + 
                         f2 * worleyPoints[2].gradient.y + 
                         f3 * worleyPoints[3].gradient.y;
            noise.b =    f0 * worleyPoints[0].gradient.z + 
                         f1 * worleyPoints[1].gradient.z + 
                         f2 * worleyPoints[2].gradient.z + 
                         f3 * worleyPoints[3].gradient.z;
            break;
        case CELLID:
            {
                const int cellId = worleyPoints[getAttrCellId(map)].id;
                noise = NOISE_worleyGetCellColor(me->mNoise, cellId);
            }
            break;
        case CELL_EDGES:
            {
                float d = worleyPoints[1].dist -
                          worleyPoints[0].dist;

                // Isolate Edges as equidistant points
                // between voronoi cell centers
                d = INTERPOLATION_smoothStep(d, 0.0f, 0.05f);

                noise = adjust(d, remap, biasVal, gainVal,
                               getAttrUseSmoothstep(map), smoothstepVal,
                               getAttrInvert(map), colorA, colorB);
            }
            break;
        case POINTS:
            {
                // 0.1 constant is to get more user-friendly values on slider
                float ps = 0.1f * (float) getAttrPointSize(map);
                ps = max(sEpsilon, ps);
                float d = worleyPoints[getAttrCellId(map)].dist;
                d = 1.0f - INTERPOLATION_smoothStep(d, ps - sEpsilon, ps);
                noise = Color_ctor(d, d, d);
            }
        break;
        default:
            break;
    }

    return noise;
}

DEFINE_MAP_SHADER(NoiseWorleyMap_v2, sample)

// Reproduction in whole or in part without prior written worleyPermission of a
