// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

///
/// @file DwaBaseLayerable.cc
/// $Id$
///

#include "DwaBaseLayerable.h"

#include <moonray/rendering/shading/MaterialApi.h>
#include <scene_rdl2/common/math/ReferenceFrame.h>
#include <scene_rdl2/render/logging/logging.h>
#include <moonray/common/mcrt_macros/moonray_static_check.h>

namespace {

// This is a construct to access the ISPC Data Struct from Inside the Scalar Material object.
// Note we are not using MATERIAL_GET_ISPC_CPTR to retrieve the ispc structure from the Material
// class as in other materials since that macro assumes a certain data layout we do not adhere to in
// DwaBase materials.
extern "C" {
const void*
getLabelsDataPtr(const void* ptr)
{
    const moonshine::dwabase::DwaBaseLayerable* classPtr = static_cast<const moonshine::dwabase::DwaBaseLayerable*>(ptr);
    return (const void*)classPtr->getLabelsDataStruct();
}

const void*
getISPCDwaBaseLayerablePtr(const void* ptr)
{
    const moonshine::dwabase::DwaBaseLayerable* classPtr = static_cast<const moonshine::dwabase::DwaBaseLayerable*>(ptr);
    return (const void*)classPtr->getISPCDwaBaseLayerableStruct();
}
}

} // anonymous namespace

namespace moonshine {
namespace dwabase {

static ispc::DwaBaseEventMessages sEventMessages;

void
DwaBaseLayerable::registerShadeTimeEventMessages()
{
    mIspc.mEventMessagesPtr = (ispc::DwaBaseEventMessages*)&sEventMessages;

   // Register shade time event messages.  We require and expect
   // these events to have the same value across all instances of the shader.
   // No conditional registration of events is allowed.
   // To allow for the possibility that we may someday create materials
   // on multiple threads, we'll protect the writes of the class statics
   // with a mutex.
   static std::mutex errorMutex;
   std::scoped_lock lock(errorMutex);
   MOONRAY_START_THREADSAFE_STATIC_WRITE

   sEventMessages.sErrorNoRefN =
       mLogEventRegistry.createEvent(scene_rdl2::logging::ERROR_LEVEL,
                                     "Unable to acquire refN which is required for glitter. "
                                     "Glitter cannot be applied");

   sEventMessages.sWarnNoRefPpartials =
       mLogEventRegistry.createEvent(scene_rdl2::logging::WARN_LEVEL,
                                     "No partial derivatives associated with refP. "
                                     "Unable to compute deformation for 'deformation compensation' feature. "
                                     "Glitter may stretch");

   sEventMessages.sErrorScatterTagMissing =
       mLogEventRegistry.createEvent(scene_rdl2::logging::ERROR_LEVEL,
                                     "scatter_tag not found in geometry. "
                                     "Hair glint variation cannot be applied. "
                                     "Please ensure that either scatter_tag is being output from the geometry, "
                                     "or remove hair glint variation by setting min twists and max twists to the same number");

   MOONRAY_FINISH_THREADSAFE_STATIC_WRITE
}

} // namespace dwabase
} // namespace moonshine



