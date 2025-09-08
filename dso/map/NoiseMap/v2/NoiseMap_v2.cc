// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file NoiseMap_v2.cc

#include "attributes.cc"
#include "NoiseMap_v2_ispc_stubs.h"

#include <moonray/common/mcrt_util/Atomic.h>
#include <moonray/common/noise/Perlin.h>
#include <moonray/common/noise/Simplex.h>
#include <moonray/map/primvar/Primvar.h>
#include <moonshine/common/interpolation/Interpolation.h>

#include <moonray/common/mcrt_macros/moonray_static_check.h>
#include <moonray/rendering/shading/MapApi.h>

#include <memory>

static ispc::StaticNoiseMapData sStaticNoiseMapData;

//----------------------------------------------------------------------------

using namespace moonshine;
using namespace scene_rdl2::math;

RDL2_DSO_CLASS_BEGIN(NoiseMap_v2, Map)
public:
    NoiseMap_v2(SceneClass const &sceneClass, std::string const &name);
    ~NoiseMap_v2();
    void update();

private:
    static void sample(const Map *self, moonray::shading::TLState *tls,
                       const moonray::shading::State &state, Color *sample);

    ispc::NoiseMap_v2 mIspc; // must be the 1st member

    std::unique_ptr<moonray::shading::Xform> mXform;
    std::unique_ptr<moonray::noise::Perlin> mNoiseDistort;
    std::unique_ptr<moonray::noise::Perlin> mNoiseR;
    std::unique_ptr<moonray::noise::Perlin> mNoiseG;
    std::unique_ptr<moonray::noise::Perlin> mNoiseB;
    std::unique_ptr<moonray::noise::Simplex> mNoiseSimplexDistort;
    std::unique_ptr<moonray::noise::Simplex> mNoiseSimplexR;
    std::unique_ptr<moonray::noise::Simplex> mNoiseSimplexG;
    std::unique_ptr<moonray::noise::Simplex> mNoiseSimplexB;

RDL2_DSO_CLASS_END(NoiseMap_v2)

NoiseMap_v2::NoiseMap_v2(SceneClass const &sceneClass,
                         std::string const &name):
    Parent(sceneClass, name)
{
    mSampleFunc = NoiseMap_v2::sample;
    mSampleFuncv = (SampleFuncv) ispc::NoiseMap_v2_getSampleFunc();

    // Store keys to shader data
    mIspc.mRefPKey = moonray::shading::StandardAttributes::sRefP;
    mIspc.mHairSurfaceSTKey = moonray::shading::StandardAttributes::sSurfaceST;
    mIspc.mHairClosestSurfaceSTKey = moonray::shading::StandardAttributes::sClosestSurfaceST;

    mIspc.mNoiseMapDataPtr =
        (ispc::StaticNoiseMapData*)&sStaticNoiseMapData;

    const auto errorMissingReferenceData = sLogEventRegistry.createEvent(scene_rdl2::logging::ERROR_LEVEL,
                                           "missing reference data");

    using namespace moonray::util;
    MOONRAY_START_THREADSAFE_STATIC_WRITE
    atomicStore(&sStaticNoiseMapData.sErrorMissingReferenceData, errorMissingReferenceData);

    const SceneVariables &sceneVariables = getSceneClass().getSceneContext()->getSceneVariables();

    auto& cppFatalColor = asCpp(sStaticNoiseMapData.sFatalColor);
    const auto& sceneVarFatalColor = sceneVariables.get(SceneVariables::sFatalColor);
    atomicStore(&cppFatalColor.r, sceneVarFatalColor.r);
    atomicStore(&cppFatalColor.g, sceneVarFatalColor.g);
    atomicStore(&cppFatalColor.b, sceneVarFatalColor.b);
    MOONRAY_FINISH_THREADSAFE_STATIC_WRITE
}

NoiseMap_v2::~NoiseMap_v2()
{
}

void
NoiseMap_v2::update()
{
    // Get input object and camera
    const Geometry* geom = get(attrObject) ?
                get(attrObject)->asA<Geometry>() : nullptr;
    const Camera* cam = get(attrCamera) ?
            get(attrCamera)->asA<Camera>() : nullptr;

    mXform.reset();
    mIspc.mXform = nullptr;
    ispc::SHADING_Space space = static_cast<ispc::SHADING_Space>(get(attrSpace));
    if (space != ispc::SHADING_SPACE_REFERENCE && space != ispc::SHADING_SPACE_INPUT_COORDINATES) {
        // Construct Xform with custom camera
        mXform = std::make_unique<moonray::shading::Xform>(this, geom, cam, nullptr);
        mIspc.mXform = mXform->getIspcXform();
    }

    // If using reference space, request reference space
    // primitive attributes from the geometry
    if (hasChanged(attrSpace)) {
        mRequiredAttributes.clear();
        mOptionalAttributes.clear();
        if (get(attrSpace) == ispc::SHADING_SPACE_REFERENCE) {
            mRequiredAttributes.push_back(mIspc.mRefPKey);
        }
        if (get(attrSpace) == ispc::SHADING_SPACE_HAIR_SURFACE_ST) {
            mRequiredAttributes.push_back(mIspc.mHairSurfaceSTKey);
        }
        if (get(attrSpace) == ispc::SHADING_SPACE_HAIR_CLOSEST_SURFACE_ST) {
            mRequiredAttributes.push_back(mIspc.mHairClosestSurfaceSTKey);
        }
    }

    // If any of the transform parameters are bound, then
    // we can't cache the transform, otherwise we do.
    if( getBinding(attrTranslation) || 
        getBinding(attrRotation) || 
        getBinding(attrScale) ||
        getBinding(attrFrequencyMultiplier)
    ){
        mIspc.mUseStaticXform = false;
    } else {
        mIspc.mUseStaticXform = true;
        asCpp(mIspc.mStaticXform) = mNoiseR->orderedCompose(
            get(attrTranslation),
            get(attrRotation) * sPi / 180.f,
            get(attrScale),
            get(attrTransformationOrder),
            get(attrRotationOrder)
        );
    }

    // Construct the noise object
    if (
        hasChanged(attrDistortion) || 
        hasChanged(attrDistortionNoiseType) || 
        hasChanged(attrSeed) || 
        hasChanged(attrNoiseType) || 
        hasChanged(attrUse4D)
    ) {
        if (hasChanged(attrDistortion) || hasChanged(attrDistortionNoiseType)) {
            if (!isZero(get(attrDistortion))) {
                if (get(attrDistortionNoiseType) == ispc::NoiseType_Simplex) {
                    mNoiseSimplexDistort = std::make_unique<moonray::noise::Simplex>(
                        get(attrSeed), get(attrUse4D));
                    mIspc.mNoiseSimplexDistort = mNoiseSimplexDistort->getIspcSimplex();
                } else {
                    mNoiseDistort = std::make_unique<moonray::noise::Perlin>(
                        get(attrSeed), 2048, false, get(attrUse4D));
                    mIspc.mNoiseDistort = mNoiseDistort->getIspcPerlin();
                }
            }
        }

        if (get(attrNoiseType) ==  ispc::NoiseType_Simplex) {
            mNoiseSimplexR = std::make_unique<moonray::noise::Simplex>(
                get(attrSeed), get(attrUse4D));
            mIspc.mNoiseSimplexR = mNoiseSimplexR->getIspcSimplex();

            if (get(attrColor)) {
                // The scene_rdl2 Random class gives the same result
                // for seed = 0 and seed = 1 so we add + 2 and + 3 here instead
                // of + 1 and + 2
                mNoiseSimplexG = std::make_unique<moonray::noise::Simplex>(
                    get(attrSeed) + 2, get(attrUse4D));
                mIspc.mNoiseSimplexG = mNoiseSimplexG->getIspcSimplex();

                mNoiseSimplexB = std::make_unique<moonray::noise::Simplex>(
                    get(attrSeed) + 3, get(attrUse4D));
                mIspc.mNoiseSimplexB = mNoiseSimplexB->getIspcSimplex();
            }
        } else {
            mNoiseR = std::make_unique<moonray::noise::Perlin>(get(attrSeed),
                                                                 ispc::NOISE_MAP_TABLE_SIZE,
                                                                 false,
                                                                 get(attrUse4D));
            mIspc.mNoiseR = mNoiseR->getIspcPerlin();

            if (get(attrColor)) {
                mNoiseG = std::make_unique<moonray::noise::Perlin>(get(attrSeed) + 2,
                                                                     ispc::NOISE_MAP_TABLE_SIZE,
                                                                     false,
                                                                     get(attrUse4D));

                mNoiseB = std::make_unique<moonray::noise::Perlin>(get(attrSeed) + 3,
                                                                     ispc::NOISE_MAP_TABLE_SIZE,
                                                                     false,
                                                                     get(attrUse4D));
                mIspc.mNoiseB = mNoiseB->getIspcPerlin();
                mIspc.mNoiseG = mNoiseG->getIspcPerlin();
            }
        }

    }
}

void
NoiseMap_v2::sample(const Map *self, moonray::shading::TLState *tls,
                 const moonray::shading::State &state, Color *sample)
{
    const NoiseMap_v2 *me = static_cast<const NoiseMap_v2 *>(self);

    // Retrieve position
    Vec3f pos;
    ispc::SHADING_Space space = static_cast<ispc::SHADING_Space>(me->get(attrSpace));
    if (space == ispc::SHADING_SPACE_TEXTURE) {
        pos = Vec3f(
            state.getSt().x, 
            state.getSt().y, 
            0.0f);
    } else if (space == ispc::SHADING_SPACE_HAIR_SURFACE_ST) {
        Vec2f uv = state.getAttribute(moonray::shading::StandardAttributes::sSurfaceST);
        pos = Vec3f(uv.x, uv.y, 0.0f);
    } else if (space == ispc::SHADING_SPACE_HAIR_CLOSEST_SURFACE_ST) {
        Vec2f uv =
                state.getAttribute(moonray::shading::StandardAttributes::sClosestSurfaceST);
        pos = Vec3f(uv.x, uv.y, 0.0f);
    } else {
        ispc::PRIMVAR_Input_Source_Mode inputSourceMode;
        Vec3f inputPosition;
        if (space == ispc::SHADING_SPACE_REFERENCE) {
            inputSourceMode = ispc::INPUT_SOURCE_MODE_REF_P_REF_N;
        } else if (space == ispc::SHADING_SPACE_INPUT_COORDINATES) {
            inputSourceMode = ispc::INPUT_SOURCE_MODE_ATTR;
            inputPosition = evalVec3f(me, attrInputTextureCoordinate, tls, state);
        } else {
            inputSourceMode = ispc::INPUT_SOURCE_MODE_P_N;
        }

        Vec3f pos_ddx, pos_ddy, pos_ddz;
        if (!primvar::getPosition(tls, state,
                                  inputSourceMode,
                                  inputPosition,
                                  me->mXform.get(),
                                  space,
                                  me->mIspc.mRefPKey,
                                  pos, pos_ddx, pos_ddy, pos_ddz)) {
            // Log missing ref_P data message
            moonray::shading::logEvent(me, sStaticNoiseMapData.sErrorMissingReferenceData);
            *sample = asCpp(sStaticNoiseMapData.sFatalColor);
            return;
        }
    }

    // Scale P by frequency multiplier
    pos *= evalFloat(self, attrFrequencyMultiplier, tls, state);

    // Further transform P using the transform parameters
    if (me->mIspc.mUseStaticXform) {
        pos = transformPoint(
            asCpp(me->mIspc.mStaticXform), 
            pos
        );
    } else {
        // mapped xforms - compose the xform for each sample
        Xform3f xform = me->mNoiseR->orderedCompose(
            evalVec3f(self, attrTranslation, tls, state),
            evalVec3f(self, attrRotation, tls, state) * sPi/180.f,
            evalVec3f(self, attrScale, tls, state),
            me->get(attrTransformationOrder),
            me->get(attrRotationOrder)
        );
        pos = transformPoint(xform, pos);
    }

    // Distortion
    const float distortion = evalFloat(self, attrDistortion, tls, state);
    if (!isZero(distortion)) {
        float distortNoise;
        if (me->get(attrDistortionNoiseType) == ispc::NoiseType_Simplex) {
            Vec3f noiseDerivatives;
            distortNoise = me->mNoiseSimplexDistort->simplex3D(pos, 0.0f, noiseDerivatives);
        } else {
            distortNoise = me->mNoiseDistort->perlin3D(pos);
        }
        pos += distortNoise * distortion;
    }

    const float maxLevel = evalFloat(self, attrMaxLevel, tls, state);
    const float lacunarity = evalFloat(self, attrLacunarity, tls, state);
    const float persistence = evalFloat(self, attrPersistence, tls, state);
    const float amplitude = evalFloat(self, attrAmplitude, tls, state);

    float noiseR, noiseG, noiseB;
    if (me->get(attrUse4D)) {
        const float time = evalFloat(self, attrTime, tls, state);
        Vec4f pos4D;
        pos4D.x = pos.x; pos4D.y = pos.y; pos4D.z = pos.z;
        pos4D.w = time;

        if (me->get(attrNoiseType) == ispc::NoiseType_Simplex) {
            const float flowAngle = evalFloat(self, attrFlowAngle, tls, state);
            const float advectionRate = evalFloat(self, attrAdvectionRate, tls, state);
            noiseR = me->mNoiseSimplexR->simplexFractal4D(pos4D,
                flowAngle, advectionRate, maxLevel, persistence, lacunarity);

            if (me->get(attrColor)) {
                noiseG = me->mNoiseSimplexG->simplexFractal4D(pos4D,
                    flowAngle, advectionRate, maxLevel, persistence, lacunarity);
                noiseB = me->mNoiseSimplexB->simplexFractal4D(pos4D,
                    flowAngle, advectionRate, maxLevel, persistence, lacunarity);
            }
        } else {
            noiseR = me->mNoiseR->perlinFractal4D(pos4D, 
                maxLevel, persistence, lacunarity);

            if (me->get(attrColor)) {
                noiseG = me->mNoiseG->perlinFractal4D(pos4D, 
                    maxLevel, persistence, lacunarity);

                noiseB = me->mNoiseB->perlinFractal4D(pos4D, 
                    maxLevel, persistence, lacunarity);
            }
        }
    } else {
        if (me->get(attrNoiseType) == ispc::NoiseType_Simplex) {
            const float flowAngle = evalFloat(self, attrFlowAngle, tls, state);
            const float advectionRate = evalFloat(self, attrAdvectionRate, tls, state);
            noiseR = me->mNoiseSimplexR->simplexFractal3D(pos,
                                                          flowAngle,
                                                          advectionRate,
                                                          maxLevel,
                                                          persistence,
                                                          lacunarity);

            if (me->get(attrColor)) {
                noiseG = me->mNoiseSimplexG->simplexFractal3D(pos,
                    flowAngle, advectionRate, maxLevel, persistence, lacunarity);

                noiseB = me->mNoiseSimplexB->simplexFractal3D(pos,
                    flowAngle, advectionRate, maxLevel, persistence, lacunarity);
            }
        } else {
             noiseR = me->mNoiseR->perlinFractal3D(pos, 
                maxLevel, persistence, lacunarity);

            if (me->get(attrColor)) {
                noiseG = me->mNoiseG->perlinFractal3D(pos, 
                    maxLevel, persistence, lacunarity);

                noiseB = me->mNoiseB->perlinFractal3D(pos, 
                    maxLevel, persistence, lacunarity);
            }
        }
    }

    // [-1,1] -> [0,1]
    noiseR = (noiseR + 1.0f) * 0.5f;
    if (me->get(attrColor)) {
        noiseG = (noiseG + 1.0f) * 0.5f;
        noiseB = (noiseB + 1.0f) * 0.5f;
    }

    // Bias/Gain
    const float biasVal = evalFloat(self, attrBias, tls, state);
    const float gainVal = evalFloat(self, attrGain, tls, state);
    noiseR = gain(bias(noiseR, biasVal), gainVal);
    if (me->get(attrColor)) {
        noiseG = gain(bias(noiseG, biasVal), gainVal);
        noiseB = gain(bias(noiseB, biasVal), gainVal);
    }

    if(me->get(attrInvert)) {
        noiseR = 1.0f - noiseR;
        if (me->get(attrColor)) {
            noiseG = 1.0f - noiseG;
            noiseB = 1.0f - noiseB;
        }
    }

    // Smoothstep
    if (me->get(attrUseSmoothstep)) {
        const Vec2f smoothStepVal =  evalVec2f(self, attrSmoothstep, tls, state);
        noiseR = interpolation::smoothStep(noiseR, smoothStepVal.x, smoothStepVal.y);
        if (me->get(attrColor)) {
            noiseG = interpolation::smoothStep(noiseG, smoothStepVal.x, smoothStepVal.y);
            noiseB = interpolation::smoothStep(noiseB, smoothStepVal.x, smoothStepVal.y);
        }
    }

    // Apply amplitude
    noiseR *= amplitude;
    if (me->get(attrColor)) {
        noiseG *= amplitude;
        noiseB *= amplitude;
    }

    if (me->get(attrColor)) {
        *sample = Color(noiseR, noiseG, noiseB);
    } else {
        const Color colorA = evalColor(self, attrColorA, tls, state);
        const Color colorB = evalColor(self, attrColorB, tls, state);
        *sample = Color(lerp(colorA, colorB, noiseR));
    }
}

