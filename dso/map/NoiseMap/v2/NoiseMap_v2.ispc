// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file NoiseMap_v2.ispc

#include "attributes.isph"

#include <moonshine/common/interpolation/ispc/Interpolation.isph>
#include <moonshine/common/noise/ispc/Perlin.isph>
#include <moonshine/common/noise/ispc/Simplex.isph>
#include <moonray/map/primvar/ispc/Primvar.isph>

#include <moonray/rendering/shading/ispc/MapApi.isph>

enum NoiseType {
    NoiseType_Classic = 0,
    NoiseType_Simplex
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(NoiseType);

struct StaticNoiseMapData {
    // shade time error events - these can be static as long as we
    // ensure they have the same value across all instances.
    // no conditional registration of log events is allowed.
    uniform Color sFatalColor;
    uniform int sErrorMissingReferenceData;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(StaticNoiseMapData);

enum NoiseMapConstants{
    NOISE_MAP_TABLE_SIZE = 2048
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(NoiseMapConstants);

struct NoiseMap_v2
{
    const Xform *mXform;
    const NOISE_Perlin *mNoiseDistort;
    const NOISE_Perlin *mNoiseR;
    const NOISE_Perlin *mNoiseG;
    const NOISE_Perlin *mNoiseB;
    const NOISE_Simplex *mNoiseSimplexDistort;
    const NOISE_Simplex *mNoiseSimplexR;
    const NOISE_Simplex *mNoiseSimplexG;
    const NOISE_Simplex *mNoiseSimplexB;
    uniform int mRefPKey;
    uniform int mHairSurfaceSTKey;
    uniform int mHairClosestSurfaceSTKey;
    uniform bool mUseStaticXform;
    uniform Xform3f mStaticXform;

    uniform StaticNoiseMapData* uniform mNoiseMapDataPtr;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(NoiseMap_v2);

static Color
sample(const uniform Map * uniform map,
       uniform ShadingTLState *uniform tls,
       const varying State &state)
{
    const uniform NoiseMap_v2 * uniform me =
        MAP_GET_ISPC_CPTR(NoiseMap_v2, map);

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
            logEvent(map, tls, me->mNoiseMapDataPtr->sErrorMissingReferenceData);
            return me->mNoiseMapDataPtr->sFatalColor;
        }
    }

    // Scale P by frequency multiplier
    pos = pos * evalAttrFrequencyMultiplier(map, tls, state);

    // Further transform P using the transform parameters
    if (me->mUseStaticXform) {
        pos = transformPoint(me->mStaticXform, pos);
    } 
    else {
        // mapped xforms - compose the xform for each sample
        Xform3f xform = NOISE_orderedCompose(evalAttrTranslation(map, tls, state),
                                             evalAttrRotation(map, tls, state) * sPi/180.f,
                                             evalAttrScale(map, tls, state),
                                             getAttrTransformationOrder(map),
                                             getAttrRotationOrder(map));
        pos = transformPoint(xform, pos);
    }

    // Perlin ultimately degrades into a series of table lookups,
    // which defeats the function and operator overloading of autodiff.
    // we'll need to use finite differences to compute the value
    // and derivatives of this function.

    // For each bindable input parameter, we evaluate the value.
    varying float lacunarity = evalAttrLacunarity(map, tls, state);
    varying float persistence = evalAttrPersistence(map, tls, state);
    varying float amplitude = evalAttrAmplitude(map, tls, state);
    varying float distortion = evalAttrDistortion(map, tls, state);
    varying float maxLevel = evalAttrMaxLevel(map, tls, state);
    varying float biasVal = evalAttrBias(map, tls, state);
    varying float gainVal = evalAttrGain(map, tls, state);
    varying float flowAngle = evalAttrFlowAngle(map, tls, state);
    varying float advectionRate = evalAttrAdvectionRate(map, tls, state);
    varying float time = evalAttrTime(map, tls, state);
    varying Vec2f smoothstepVal = evalAttrSmoothstep(map, tls, state);

    varying float noiseR;
    varying float noiseG;
    varying float noiseB;

    // Apply distortion parameter to P before it is 
    // input into the noise function
    if (!isZero(distortion)) {
        float distortNoise;
        if (getAttrDistortionNoiseType(map) == NoiseType_Simplex) {
            Vec3f noiseDerivatives;
            distortNoise = NOISE_simplex3D(me->mNoiseSimplexDistort, pos, 0.0f, noiseDerivatives);
        } else {
            distortNoise = NOISE_perlin3D(me->mNoiseDistort, pos);
        }
        pos.x += distortNoise * distortion;
        pos.y += distortNoise * distortion;
        pos.z += distortNoise * distortion;
    }

    if (getAttrUse4D(map)) {
        Vec4f pos4D;
        pos4D.x = pos.x; pos4D.y = pos.y; pos4D.z = pos.z;
        pos4D.w = time;
        if (getAttrNoiseType(map) == NoiseType_Simplex) {
            noiseR = NOISE_simplexFractal4D(
                me->mNoiseSimplexR, pos4D, 
                flowAngle, advectionRate, 
                maxLevel, 
                persistence, lacunarity);

            if (getAttrColor(map)) {
                noiseG = NOISE_simplexFractal4D(
                    me->mNoiseSimplexG, pos4D, 
                    flowAngle, advectionRate, 
                    maxLevel, 
                    persistence, lacunarity);

                noiseB = NOISE_simplexFractal4D(
                    me->mNoiseSimplexB, pos4D, 
                    flowAngle, advectionRate, 
                    maxLevel, 
                    persistence, lacunarity);
            }
        } else {
            noiseR = NOISE_perlinFractal4D(
                me->mNoiseR, pos4D, 
                maxLevel, 
                persistence, lacunarity);

            if (getAttrColor(map)) {
                noiseG = NOISE_perlinFractal4D(
                    me->mNoiseG, pos4D, 
                    maxLevel, 
                    persistence, lacunarity);

                noiseB = NOISE_perlinFractal4D(
                    me->mNoiseB, pos4D, 
                    maxLevel, 
                    persistence, lacunarity);
            }
        }
    } else {
        if (getAttrNoiseType(map) == NoiseType_Simplex) {
            noiseR = NOISE_simplexFractal3D(
                me->mNoiseSimplexR, pos, 
                flowAngle, advectionRate, 
                maxLevel, 
                persistence, lacunarity);

            if (getAttrColor(map)) {
                noiseG = NOISE_simplexFractal3D(
                    me->mNoiseSimplexG, pos, 
                    flowAngle, advectionRate, 
                    maxLevel, 
                    persistence, lacunarity);

                noiseB = NOISE_simplexFractal3D(
                    me->mNoiseSimplexB, pos, 
                    flowAngle, advectionRate, 
                    maxLevel, 
                    persistence, lacunarity);
            }
        } else {
            noiseR = NOISE_perlinFractal3D(
                me->mNoiseR, pos,
                maxLevel, 
                persistence, lacunarity);

            if (getAttrColor(map)) {
                noiseG = NOISE_perlinFractal3D(
                    me->mNoiseG, pos,
                    maxLevel, 
                    persistence, lacunarity);

                noiseB = NOISE_perlinFractal3D(
                    me->mNoiseB, pos,
                    maxLevel, 
                    persistence, lacunarity);
            }
        }
    }

    // [-1,1] -> [0,1]
    noiseR = (noiseR + 1.0f) * 0.5f;
    if (getAttrColor(map)) {
        noiseG = (noiseG + 1.0f) * 0.5f;
        noiseB = (noiseB + 1.0f) * 0.5f;
    }

    // Bias/Gain
    noiseR = gain(bias(noiseR, biasVal), gainVal);
    if (getAttrColor(map)) {
        noiseG = gain(bias(noiseG, biasVal), gainVal);
        noiseB = gain(bias(noiseB, biasVal), gainVal);
    }

    if(getAttrInvert(map)) {
        noiseR = 1.0f - noiseR;
        if (getAttrColor(map)) {
            noiseG = 1.0f - noiseG;
            noiseB = 1.0f - noiseB;
        }
    }

    // Smoothstep
    if (getAttrUseSmoothstep(map)) {
        noiseR = INTERPOLATION_smoothStep(noiseR, smoothstepVal.x, smoothstepVal.y);
        if (getAttrColor(map)) {
            noiseG = INTERPOLATION_smoothStep(noiseG, smoothstepVal.x, smoothstepVal.y);
            noiseB = INTERPOLATION_smoothStep(noiseB, smoothstepVal.x, smoothstepVal.y);
        }
    }

    // Apply amplitude
    noiseR *= amplitude;
    if (getAttrColor(map)) {
        noiseG *= amplitude;
        noiseB *= amplitude;
    }

    if (getAttrColor(map)) {
        return Color_ctor(noiseR, noiseG, noiseB);
    } else {
        Color colorA = evalAttrColorA(map, tls, state);
        Color colorB = evalAttrColorB(map, tls, state);
        return lerp(colorA, colorB, noiseR);
    }
}

DEFINE_MAP_SHADER(NoiseMap_v2, sample)
               
