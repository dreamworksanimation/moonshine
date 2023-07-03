// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file GradientMap.cc

#include "attributes.cc"
#include "GradientMap_ispc_stubs.h"

#include <moonray/common/mcrt_macros/moonray_static_check.h>
#include <moonray/rendering/shading/MapApi.h>
#include <scene_rdl2/render/util/stdmemory.h>

using namespace moonray::shading;
using namespace scene_rdl2::math;

static ispc::StaticGradientMapData sStaticGradientMapData;

namespace {

static const int sFalloffHead = 1;
static const int sFalloffTail = 7;

// f, g, and h, are helper functions for naturalFalloff().
float
f(const float x, const float power)
{
    return 1.0f / scene_rdl2::math::pow(x, power);
}

float
g(const float x, const float power)
{
    return f(sFalloffTail * x + sFalloffHead, power);
}

float
h(const float x, const float power)
{
    return g(x, power) - g(1.0f, power);
}

// Helper function for NATURAL falloff type.
float
naturalFalloff(const float x, const float power)
{
    return h(x, power) / h(0.0f, power);
}
float
computeFalloff(float t, const ispc::GradientFalloffType type, const float exponent)
{
    switch (type) {
    case ispc::GRADIENT_FALLOFF_NONE:
        t = 1.f;
        break;
    case ispc::GRADIENT_FALLOFF_NATURAL:
        t = naturalFalloff(t, exponent);
        break;
    case ispc::GRADIENT_FALLOFF_LINEAR:
        t = scene_rdl2::math::pow(t, exponent);
        break;
    case ispc::GRADIENT_FALLOFF_SQUARED:
        t *= t;
        break;
    case ispc::GRADIENT_FALLOFF_GAUSSIAN:
        t = scene_rdl2::math::pow(static_cast<float>((scene_rdl2::math::cos((1.0f - t) * sPi) + 1.0f) / 2.0f), exponent);
        break;
    case ispc::GRADIENT_FALLOFF_EASE_OUT:
        t = 3.0f * t * t - 2.0f * t * t * t;
        t = scene_rdl2::math::pow(t, exponent);
        break;
    }
    return t;
}
} // end anon namespace

//----------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(GradientMap, scene_rdl2::rdl2::Map)

public:
    GradientMap(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name);
    ~GradientMap() override;
    void update() override;

private:
    static void sample(const scene_rdl2::rdl2::Map *self, moonray::shading::TLState *tls,
                       const moonray::shading::State &state, Color *sample);

    bool verifyInputs();

    ispc::GradientMap mIspc; // Must be the 1st member.

    std::unique_ptr<moonray::shading::Xform> mXform;

RDL2_DSO_CLASS_END(GradientMap)

//----------------------------------------------------------------------------

GradientMap::GradientMap(const scene_rdl2::rdl2::SceneClass &sceneClass,
                         const std::string &name):
    Parent(sceneClass, name)
{
    mSampleFunc = GradientMap::sample;
    mSampleFuncv = (scene_rdl2::rdl2::SampleFuncv) ispc::GradientMap_getSampleFunc();

    // Store keys to shader data
    mIspc.mRefPKey = moonray::shading::StandardAttributes::sRefP;

    mIspc.mGradientMapDataPtr = (ispc::StaticGradientMapData*)&sStaticGradientMapData;

     // register shade time event messages.  we require and expect
    // these events to have the same value across all instances of the shader.
    // no conditional registration of events is allowed.
    // to allow for the possibility that we may someday create image maps
    // on multiple threads, we'll protect the writes of the class statics
    // with a mutex.
    static std::mutex errorMutex;
    std::scoped_lock lock(errorMutex);
    MOONRAY_START_THREADSAFE_STATIC_WRITE
    sStaticGradientMapData.sErrorMissingReferenceData =
        mLogEventRegistry.createEvent(scene_rdl2::logging::ERROR_LEVEL,
                                      "missing reference data");
    MOONRAY_FINISH_THREADSAFE_STATIC_WRITE
}

GradientMap::~GradientMap()
{
}

bool
GradientMap::verifyInputs() {
    if (get(attrFalloffType) > 5 || get(attrFalloffType) < 0) {
        fatal("Gradient: Unsupported falloff type");
        return false;
    }
    if (get(attrSpace) > 6 || get(attrSpace) < 0) {
        fatal("Gradient: Unsupported space");
        return false;
    }
    const scene_rdl2::rdl2::Geometry* geom = get(attrObject) ?
                    get(attrObject)->asA<scene_rdl2::rdl2::Geometry>() : nullptr;
    if (geom != nullptr && get(attrSpace) != ispc::SHADING_SPACE_OBJECT) {
        warn("Gradient: To use the bound object's space, space must be set to object mode");
    }

    return true;
}

void
GradientMap::update()
{
    // Set fatal color
    const scene_rdl2::rdl2::SceneVariables &sv = getSceneClass().getSceneContext()->getSceneVariables();
    mIspc.mFatalColor = asIspc(sv.get(scene_rdl2::rdl2::SceneVariables::sFatalColor));

    if (!verifyInputs()) {
        fatal("Gradient input validation failed");
        return;
    }

    if (hasChanged(attrSpace)) {
        mRequiredAttributes.clear();
        mOptionalAttributes.clear();
        if (get(attrSpace) == ispc::SHADING_SPACE_REFERENCE) {
            mRequiredAttributes.push_back(mIspc.mRefPKey);
        }
    }

    // If a separate object is specified, get the input object.
    // Only relevant if SHADING_SPACE_OBJECT is chosen.
    const scene_rdl2::rdl2::Geometry* geom = get(attrObject) ?
                get(attrObject)->asA<scene_rdl2::rdl2::Geometry>() : nullptr;

    // Construct Xform with default transforms for camera and screen.
    mXform = fauxstd::make_unique<moonray::shading::Xform>(this, geom, nullptr, nullptr);
    mIspc.mXform = mXform->getIspcXform();
}

void
GradientMap::sample(const scene_rdl2::rdl2::Map *self,
                    moonray::shading::TLState *tls,
                    const moonray::shading::State &state,
                    Color *sample)
{
    const GradientMap* me = static_cast<const GradientMap*>(self);

    const Color colorA = evalColor(me, attrColorA, tls, state);
    const Color colorB = evalColor(me, attrColorB, tls, state);
    const Vec3f start = me->get(attrStart);
    const Vec3f end = me->get(attrEnd);
    const float falloffStart = me->get(attrFalloffStart);
    const float falloffEnd = me->get(attrFalloffEnd);
    const int falloffType = me->get(attrFalloffType);
    const float falloffExponent = me->get(attrFalloffExponent);
    const float falloffEndIntensity = me->get(attrFalloffEndIntensity);
    const float falloffBias = me->get(attrFalloffBias);
    const bool isSymmetric = me->get(attrSymmetric);
    float symmetricCenter = me->get(attrSymmetricCenter);

    // Retrieve position
    Vec3f pos;
    const int space = me->get(attrSpace);
    if (space == ispc::SHADING_SPACE_TEXTURE) {
        pos = Vec3f(
            state.getSt().x,
            state.getSt().y,
            0.0f
        );
    } else if (space == ispc::SHADING_SPACE_REFERENCE) {
        state.getRefP(pos);
    } else {
        pos = state.getP();
        if (space != ispc::SHADING_SPACE_RENDER) {
            pos = me->mXform->transformPoint(ispc::SHADING_SPACE_RENDER,
                                             space,
                                             state,
                                             pos);
        }
    }

    // blend is the scalar projection of P onto v.
    float blend = 0.0f;
    Vec3f v = end - start;
    float magSq = lengthSqr(v);

    if (magSq > 0.0f) {
        Vec3f ps = pos - start;
        blend = saturate(dot(ps, v) / magSq);
    }

    // Remap blend value between falloffStart and falloffEnd values.
    if (blend < falloffStart) {
        blend = 0.0f;
    } else if (blend > falloffEnd) {
        blend = 1.0f;
    } else {
        blend = saturate((blend - falloffStart) / (falloffEnd - falloffStart));
    }

    // Natural falloff curve is reversed from others, so we don't flip it.
    // FIXME: this is a little craziness, just fix the function instead?
    if (falloffType != ispc::GRADIENT_FALLOFF_NATURAL) blend = 1.0f - blend;
    blend = (1.0f - computeFalloff(blend, static_cast<ispc::GradientFalloffType>(falloffType), falloffExponent));

    // colorA blends into colorB and then back into colorA.
    // The blend from colorA to colorB and back to colorA
    // occurs in the same amount of distance as blending from
    // colorA to colorB without symmetric mode on.
    if (isSymmetric) {
        // Clamp to exclusive (0,1) range.
        symmetricCenter = clamp(symmetricCenter, sEpsilon, sOneMinusEpsilon);

        if (blend < symmetricCenter) {
            blend /= symmetricCenter;
        } else {
            blend = 1.0f - (blend - symmetricCenter) / (1.0f - symmetricCenter);
        }
    }
    blend *= saturate(falloffEndIntensity);
    blend = bias(blend, falloffBias);
    Color result(lerp(colorA, colorB, blend));

    *sample = result;
}

