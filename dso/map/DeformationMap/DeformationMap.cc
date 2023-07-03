// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file DeformationMap.cc

#include "attributes.cc"
#include "DeformationMap_ispc_stubs.h"

#include <moonray/common/mcrt_macros/moonray_static_check.h>
#include <moonray/rendering/shading/MapApi.h>

static ispc::StaticDeformationMapData sStaticDeformationMapData;

using namespace moonray;
using namespace scene_rdl2::math;
using namespace moonray::shading;

//---------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(DeformationMap, scene_rdl2::rdl2::Map)

public:
    DeformationMap(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name);
    ~DeformationMap();
    virtual void update() override;

private:
    static void sample(const scene_rdl2::rdl2::Map* self,
                       TLState *tls,
                       const State& state,
                       Color* sample);

    ispc::DeformationMap mIspc; // must be first member

RDL2_DSO_CLASS_END(DeformationMap)

//---------------------------------------------------------------------------

DeformationMap::DeformationMap(const scene_rdl2::rdl2::SceneClass& sceneClass,
                               const std::string& name) :
    Parent(sceneClass, name)
{
    mSampleFunc = DeformationMap::sample;
    mSampleFuncv = (scene_rdl2::rdl2::SampleFuncv) ispc::DeformationMap_getSampleFunc();

    mIspc.mDataPtr = (ispc::StaticDeformationMapData*)&sStaticDeformationMapData;
    mIspc.mRefPKey = moonray::shading::StandardAttributes::sRefP;

    // Get fatal color for error state
    const scene_rdl2::rdl2::SceneVariables &sv = getSceneClass().getSceneContext()->getSceneVariables();
    asCpp(mIspc.mFatalColor) = sv.get(scene_rdl2::rdl2::SceneVariables::sFatalColor);

    // Register shade time event messages.  We require and expect
    // these events to have the same value across all instances of the shader.
    // no conditional registration of events is allowed.
    // To allow for the possibility that we may someday create image maps
    // on multiple threads, we'll protect the writes of the class statics
    // with a mutex.
    static std::mutex errorMutex;
    std::scoped_lock lock(errorMutex);

    MOONRAY_START_THREADSAFE_STATIC_WRITE

    sStaticDeformationMapData.sErrorZeroDerivatives =
        mLogEventRegistry.createEvent(scene_rdl2::logging::ERROR_LEVEL,
                                      "Unable to compute deformation.  One of the derivatives is zero");

    sStaticDeformationMapData.sErrorZeroRefDerivatives =
        mLogEventRegistry.createEvent(scene_rdl2::logging::ERROR_LEVEL,
                                      "Unable to compute deformation.  One of the reference derivatives is zero");

    sStaticDeformationMapData.sWarningZeroDerivatives =
        mLogEventRegistry.createEvent(scene_rdl2::logging::WARN_LEVEL,
                                      "Unable to compute deformation.  One of the derivatives is zero");

    sStaticDeformationMapData.sWarningZeroRefDerivatives =
        mLogEventRegistry.createEvent(scene_rdl2::logging::WARN_LEVEL,
                                      "Unable to compute deformation.  One of the reference derivatives is zero");

    MOONRAY_FINISH_THREADSAFE_STATIC_WRITE
}

DeformationMap::~DeformationMap()
{
}

void
DeformationMap::update()
{
    mRequiredAttributes.push_back(mIspc.mRefPKey);
}

void
DeformationMap::sample(const scene_rdl2::rdl2::Map* self,
                       TLState *tls,
                       const State& state,
                       Color* sample)
{
    const DeformationMap* me = static_cast<const DeformationMap*>(self);

    // Current Space Vectors
    const Vec3f curdPds = state.getdPds();
    const Vec3f curdPdt = state.getdPdt();

    Vec3f curZ = cross(curdPds, curdPdt);
    const float lengthCurZ = length(curZ);

    // Errors if either curdPds or curdPdt are zero vectors or if they are identical.
    if (isZero(lengthCurZ)) {
        if (me->get(attrUseWarningColor)) {
            logEvent(me, tls, me->mIspc.mDataPtr->sWarningZeroDerivatives);
            *sample = evalColor(me, attrWarningColor, tls, state);
        } else {
            logEvent(me, tls, me->mIspc.mDataPtr->sErrorZeroDerivatives);
            *sample = asCpp(me->mIspc.mFatalColor);
        }
        return;
    }

    curZ = curZ / lengthCurZ;
    const Vec3f curX = normalize(curdPds);
    const Vec3f curY = normalize(cross(curZ, curX));

    // Reference Space Vectors
    Vec3f refdPds, refdPdt;
    state.getdVec3fAttrds(me->mIspc.mRefPKey, refdPds);
    state.getdVec3fAttrdt(me->mIspc.mRefPKey, refdPdt);

    Vec3f refZ = cross(refdPds, refdPdt);
    const float lengthRefZ = length(refZ);

    // Errors if either refdPds or refdPdt are zero vectors or if they are identical.
    if (isZero(lengthRefZ)) {
        if (me->get(attrUseWarningColor)) {
            logEvent(me, tls, me->mIspc.mDataPtr->sWarningZeroRefDerivatives);
            *sample = evalColor(me, attrWarningColor, tls, state);
        } else {
            logEvent(me, tls, me->mIspc.mDataPtr->sErrorZeroRefDerivatives);
            *sample = asCpp(me->mIspc.mFatalColor);
        }
        return;
    }

    refZ = refZ / lengthRefZ;
    const Vec3f refX = normalize(refdPds);
    const Vec3f refY = normalize(cross(refZ, refX));

    // Stretch/Compression Factors
    const float deformationS = scene_rdl2::math::abs(dot(curdPds, curX) /
                                                     dot(refdPds, refX));
    const float deformationT = scene_rdl2::math::abs(dot(curdPdt, curY) /
                                                     dot(refdPdt, refY));
    const float deformationAvg = length(cross(curdPds, curdPdt)) /
                                 length(cross(refdPds, refdPdt));

    Color result;
    switch (me->get(attrOutputMode)) {
    case ispc::OutputMode::RGB:
        result = Color(deformationS, deformationT, deformationAvg);
        break;
    case ispc::OutputMode::DEFORMATION_S:
        result = Color(deformationS, deformationS, deformationS);
        break;
    case ispc::OutputMode::DEFORMATION_T:
        result = Color(deformationT, deformationT, deformationT);
        break;
    default:
        result = Color(deformationAvg, deformationAvg, deformationAvg);
    }
    *sample = result;
}

