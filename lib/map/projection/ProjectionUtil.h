// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#pragma once

#include <moonray/rendering/shading/Shading.h>

#include <scene_rdl2/render/logging/logging.h>
#include <scene_rdl2/scene/rdl2/rdl2.h>

#include "Projection_ispc_stubs.h"

namespace moonshine  {
namespace projection  {

void
initLogEvents(ispc::PROJECTION_StaticData& staticData,
              scene_rdl2::logging::LogEventRegistry<scene_rdl2::rdl2::Shader>& logEventRegistry,
              const scene_rdl2::rdl2::Shader * shader);

std::unique_ptr<moonray::shading::Xform>
getProjectorXform(const scene_rdl2::rdl2::SceneObject *shader,
                  const ispc::PROJECTION_Mode projectionMode,
                  const scene_rdl2::rdl2::SceneObject* projectorObject,
                  const scene_rdl2::math::Mat4d &projectionMatrix,
                  const ispc::PROJECTION_TransformOrder transformOrder,
                  const ispc::PROJECTION_RotationOrder rotationOrder,
                  const scene_rdl2::math::Vec3f& translation,
                  const scene_rdl2::math::Vec3f& rotation,
                  const scene_rdl2::math::Vec3f& scale);

} // projection
} // moonshine


