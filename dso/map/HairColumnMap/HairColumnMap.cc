// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file HairColumnMap.cc

#include "attributes.cc"
#include "HairColumnMap_ispc_stubs.h"

#include <moonray/common/mcrt_macros/moonray_static_check.h>
#include <moonray/rendering/shading/MapApi.h>

using namespace moonray::shading;
using namespace scene_rdl2::math;
//----------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(HairColumnMap, Map)

public:
    HairColumnMap(const SceneClass &sceneClass, const std::string &name);
    ~HairColumnMap() override;
    void update() override;

private:
    static void sample(const Map *self, moonray::shading::TLState *tls,
                       const moonray::shading::State &state, Color *sample);

    static scene_rdl2::logging::LogEvent sErrorScatterTagMissing;

    ispc::HairColumnMap mData;

RDL2_DSO_CLASS_END(HairColumnMap)

scene_rdl2::logging::LogEvent HairColumnMap::sErrorScatterTagMissing;

//----------------------------------------------------------------------------

HairColumnMap::HairColumnMap(const SceneClass &sceneClass, const std::string &name):
    Parent(sceneClass, name)
{
    mRequiredAttributes.push_back(moonray::shading::StandardAttributes::sScatterTag);
    mData.mScatterTagKey = moonray::shading::StandardAttributes::sScatterTag;

    mSampleFunc = HairColumnMap::sample;
    mSampleFuncv = (SampleFuncv) ispc::HairColumnMap_getSampleFunc();

    // register shade time event messages.  we require and expect
    // these events to have the same value across all instances of the shader.
    // no conditional registration of events is allowed.
    // to allow for the possibility that we may someday create image maps
    // on multiple threads, we'll protect the writes of the class statics
    // with a mutex.
    static std::mutex errorMutex;
    std::scoped_lock lock(errorMutex);
    MOONRAY_START_THREADSAFE_STATIC_WRITE
    sErrorScatterTagMissing =
        mLogEventRegistry.createEvent(scene_rdl2::logging::ERROR_LEVEL,
                                      "scatter tag not found in geometry.");
    MOONRAY_FINISH_THREADSAFE_STATIC_WRITE
}

HairColumnMap::~HairColumnMap()
{
}

void
HairColumnMap::update()
{
}

void
HairColumnMap::sample(const Map *self, moonray::shading::TLState *tls,
                        const moonray::shading::State &state, Color *sample)
{
    const HairColumnMap* me = static_cast<const HairColumnMap*>(self);

    // scatter_tag is a random number [0, 1) assigned to each hair.
    // It is used to randomly sample the u dimension of the hair column uv map in a "columnwise" fashion.
    // Hair UVs normally treat u as the hair length so to sample the column uv
    // map vertically (columnwise) the u coordinate is used as the v coordinate
    // so hair length is represented by the v dimension.
    float scatterTag = state.getAttribute(
            TypedAttributeKey<float>(me->mData.mScatterTagKey));
    if (isEqual(scatterTag, -1.f)) {
        moonray::shading::logEvent(me, tls, sErrorScatterTagMissing);
        *sample = sBlack;
    } else {
        Vec2f st = state.getSt();
        Vec2f uv(scatterTag, st[0]);

        *sample = Color(uv[0], uv[1], 0.f);
    }
}

