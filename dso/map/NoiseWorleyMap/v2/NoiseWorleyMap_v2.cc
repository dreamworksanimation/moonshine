// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

// Reproduction in whole or in part without prior written worleyPermission of a

/// @file NoiseWorleyMap_v2.cc

// Transform to specified space
#include "attributes.cc"
#include "NoiseWorleyMap_v2_ispc_stubs.h"

#include <moonshine/common/interpolation/Interpolation.h>
#include <moonshine/common/noise/Worley.h>
#include <moonray/map/primvar/Primvar.h>

#include <moonray/common/mcrt_macros/moonray_static_check.h>
#include <moonray/rendering/shading/MapApi.h>
#include <scene_rdl2/render/util/stdmemory.h>

using namespace moonray;
using namespace moonshine;
using namespace scene_rdl2::math;

static ispc::StaticNoiseWorleyMapData sStaticNoiseWorleyMapData;

//----------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(NoiseWorleyMap_v2, Map)
public:
    NoiseWorleyMap_v2(SceneClass const &sceneClass, std::string const &name);
    ~NoiseWorleyMap_v2();
    void update();

private:
    static void sample(const Map *self, moonray::shading::TLState *tls,
                       const moonray::shading::State &state, Color *sample);

    static Color adjust(float d, const Map *self,
                              moonray::shading::TLState *tls, const moonray::shading::State &state);

    ispc::NoiseWorleyMap_v2 mIspc; // must be the 1st member

    std::unique_ptr<moonray::shading::Xform> mXform;
    std::unique_ptr<noise::Worley> mNoise;

RDL2_DSO_CLASS_END(NoiseWorleyMap_v2)

// Constructor
NoiseWorleyMap_v2::NoiseWorleyMap_v2(SceneClass const &sceneClass, std::string const &name):
    Parent(sceneClass, name)
{
    mSampleFunc = NoiseWorleyMap_v2::sample;
    mSampleFuncv = (SampleFuncv) ispc::NoiseWorleyMap_v2_getSampleFunc();

    // Store keys to shader data
    mIspc.mRefPKey = moonray::shading::StandardAttributes::sRefP;
    mIspc.mHairSurfaceSTKey =
            moonray::shading::StandardAttributes::sSurfaceST;
    mIspc.mHairClosestSurfaceSTKey =
            moonray::shading::StandardAttributes::sClosestSurfaceST;

    mIspc.mNoiseWorleyMapDataPtr =
        (ispc::StaticNoiseWorleyMapData*)&sStaticNoiseWorleyMapData;

    // register shade time event messages.  we require and expect
    // these events to have the same value across all instances of the shader.
    // no conditional registration of events is allowed.
    // to allow for the possibility that we may someday create image maps
    // on multiple threads, we'll protect the writes of the class statics
    // with a mutex.
    static std::mutex errorMutex;
    std::scoped_lock lock(errorMutex);
    MOONRAY_START_THREADSAFE_STATIC_WRITE
    sStaticNoiseWorleyMapData.sErrorMissingReferenceData =
        mLogEventRegistry.createEvent(scene_rdl2::logging::ERROR_LEVEL,
                                      "missing reference data");

    const SceneVariables &sceneVariables =
        getSceneClass().getSceneContext()->getSceneVariables();
    asCpp(sStaticNoiseWorleyMapData.sFatalColor) =
        sceneVariables.get(SceneVariables::sFatalColor);
    MOONRAY_FINISH_THREADSAFE_STATIC_WRITE
}

// Destructor
NoiseWorleyMap_v2::~NoiseWorleyMap_v2()
{
}

void
NoiseWorleyMap_v2::update()
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
        mXform = fauxstd::make_unique<moonray::shading::Xform>(this, geom, cam, nullptr);
        mIspc.mXform = mXform->getIspcXform();
    }

    // If any of the transform parameters are bound, then
    // we can't cache the transform, otherwise we do.
    if( getBinding(attrTranslation) || 
        getBinding(attrRotation) || 
        getBinding(attrScale) ||
        getBinding(attrFrequency)
    ){
        mIspc.mUseStaticXform = false;
    } else {
        mIspc.mUseStaticXform = true;
        asCpp(mIspc.mStaticXform) = mNoise->orderedCompose(
            get(attrTranslation),
            get(attrRotation) * sPi / 180.f,
            get(attrScale),
            get(attrTransformationOrder),
            get(attrRotationOrder)
        );
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

    // Construct the noise object
    if (hasChanged(attrSeed)) {
        mNoise = fauxstd::make_unique<noise::Worley>(get(attrSeed),
                                                     ispc::NOISE_WORLEY_MAP_TABLE_SIZE,
                                                     ispc::NOISE_WORLEY_V2,
                                                     get(attrDistanceMethod));

        mIspc.mNoise = mNoise->getIspcWorley();
    }
}

Color
NoiseWorleyMap_v2::adjust(float d, const Map *self, moonray::shading::TLState *tls,
    const moonray::shading::State &state)
{
    const NoiseWorleyMap_v2 *me = static_cast<const NoiseWorleyMap_v2 *>(self);
    // Remap
    const Vec2f remapVal =  evalVec2f(self, attrRemap, tls, state);
    float denom = remapVal.y - remapVal.x;
    d = (d - remapVal.x) / ((denom != 0.f) ? denom : 0.000001f);

    // Bias/Gain
    const float biasVal = evalFloat(self, attrBias, tls, state);
    const float gainVal = evalFloat(self, attrGain, tls, state);
    d = scene_rdl2::math::gain(scene_rdl2::math::bias(d, biasVal), gainVal);

    // Smoothstep
    if (me->get(attrUseSmoothstep)) {
        const Vec2f smoothStepVal =  evalVec2f(self, attrSmoothstep, tls, state);
        d = interpolation::smoothStep(d, smoothStepVal.x, smoothStepVal.y);
    }

    const Color colorA =  evalColor(self, attrColorA, tls, state);
    const Color colorB =  evalColor(self, attrColorB, tls, state);
    Color noise;
    noise.r = colorA.r * (1.0 - d) + colorB.r * d;
    noise.g = colorA.g * (1.0 - d) + colorB.g * d;
    noise.b = colorA.b * (1.0 - d) + colorB.b * d;

    if (me->get(attrInvert)) {
        noise.r = 1.0f - noise.r;
        noise.g = 1.0f - noise.g;
        noise.b = 1.0f - noise.b;
    }

    return noise;
}

void
NoiseWorleyMap_v2::sample(const Map *self, moonray::shading::TLState *tls,
    const moonray::shading::State &state, Color *sample)
{
    const NoiseWorleyMap_v2 *me = static_cast<const NoiseWorleyMap_v2 *>(self);

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
            moonray::shading::logEvent(me, tls, sStaticNoiseWorleyMapData.sErrorMissingReferenceData);
            *sample = asCpp(sStaticNoiseWorleyMapData.sFatalColor);
            return;
        }
    }

    // Scale P by frequency multiplier
    pos *= evalFloat(self, attrFrequency, tls, state);

    // Further transform P using the transform parameters
    if (me->mIspc.mUseStaticXform) {
        pos = transformPoint(
            asCpp(me->mIspc.mStaticXform), 
            pos
        );
    } else {
        // mapped xforms - compose the xform for each sample
        Xform3f xform = me->mNoise->orderedCompose(
            evalVec3f(self, attrTranslation, tls, state),
            evalVec3f(self, attrRotation, tls, state) * sPi/180.f,
            evalVec3f(self, attrScale, tls, state),
            me->get(attrTransformationOrder),
            me->get(attrRotationOrder)
        );
        pos = transformPoint(xform, pos);
    }

    const float maxLevel = evalFloat(self, attrMaxLevel, tls, state);
    float minkowskiNumber = evalFloat(self, attrMinkowskiNumber, tls, state);
    // Clamp jitter to 0-1
    const float jitter = clamp(evalFloat(self, attrJitter, tls, state), 0.f, 1.f);

    const float f0 =  evalFloat(self, attrF1, tls, state);
    const float f1 =  evalFloat(self, attrF2, tls, state);
    const float f2 =  evalFloat(self, attrF3, tls, state);
    const float f3 =  evalFloat(self, attrF4, tls, state);

    noise::Worley_PointArray worleyPoints;
    ispc::NOISE_WorleySample noiseSample;
    asCpp(noiseSample.position) = pos;
    noiseSample.radius = 0.0f;
    me->mNoise->searchPointsFractal(jitter, minkowskiNumber, maxLevel, noiseSample, worleyPoints);

    Color noise(0.0f, 0.0f, 0.0f);

    switch(me->get(attrOutputMode)) {
        case ispc::DISTANCE:
            {
                float d = f0 * worleyPoints[0].dist +
                          f1 * worleyPoints[1].dist +
                          f2 * worleyPoints[2].dist +
                          f3 * worleyPoints[3].dist;
                noise = adjust(d, self, tls, state);
            }
            break;
        case ispc::GRADIENT:
            noise.r = f0 * worleyPoints[0].gradient.x +
                      f1 * worleyPoints[1].gradient.x +
                      f2 * worleyPoints[2].gradient.x +
                      f3 * worleyPoints[3].gradient.x;
            noise.g = f0 * worleyPoints[0].gradient.y +
                      f1 * worleyPoints[1].gradient.y +
                      f2 * worleyPoints[2].gradient.y +
                      f3 * worleyPoints[3].gradient.y;
            noise.b = f0 * worleyPoints[0].gradient.z +
                      f1 * worleyPoints[1].gradient.z +
                      f2 * worleyPoints[2].gradient.z +
                      f3 * worleyPoints[3].gradient.z;
            break;
        case ispc::CELLID:
            {
                const int cellId = worleyPoints[me->get(attrCellId)].id;
                noise = me->mNoise->getCellColor(cellId);
            }
            break;
        case ispc::CELL_EDGES:
            {
                float d = worleyPoints[1].dist -
                          worleyPoints[0].dist;
                // Isolate Edges as equidistant points
                // between voronoi cell centers
                d = interpolation::smoothStep(d, 0.0f, 0.05f);
                noise = adjust(d, self, tls, state);
            }
            break;
        case ispc::POINTS:
            {
                // 0.1 constant is to get more user-friendly values on slider
                float ps = 0.1f * (float) me->get(attrPointSize);
                ps = max(sEpsilon, ps);
                float d = worleyPoints[me->get(attrCellId)].dist;
                d = 1.0f - interpolation::smoothStep(d, ps - sEpsilon, ps);
                noise = Color(d, d, d);
            }
            break;
        default:
            break;
    }

    *sample = noise;
}

// Reproduction in whole or in part without prior written worleyPermission of a
