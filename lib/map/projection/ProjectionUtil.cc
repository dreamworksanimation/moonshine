// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
#include "ProjectionUtil.h"

#include <moonray/common/mcrt_macros/moonray_static_check.h>
#include <scene_rdl2/render/util/stdmemory.h>

using namespace moonray;
using namespace scene_rdl2::math;

namespace moonshine {
namespace projection {

void
initLogEvents(ispc::PROJECTION_StaticData& staticData,
              scene_rdl2::logging::LogEventRegistry& logEventRegistry,
              const scene_rdl2::rdl2::Shader * shader)
{
    // register shade time event messages.  we require and expect
    // these events to have the same value across all instances of the shader.
    // no conditional registration of events is allowed.
    // to allow for the possibility that we may someday create image maps
    // on multiple threads, we'll protect the writes of the class statics
    // with a mutex.
    static std::mutex errorMutex;
    std::scoped_lock lock(errorMutex);
    MOONRAY_START_THREADSAFE_STATIC_WRITE

    staticData.sErrorMissingProjector = logEventRegistry.createEvent(scene_rdl2::logging::ERROR_LEVEL,
        "missing or invalid projector object");

    staticData.sErrorMissingRefP = logEventRegistry.createEvent(scene_rdl2::logging::ERROR_LEVEL,
        "ref_P primitive attribute is missing");

    staticData.sErrorMissingRefN = logEventRegistry.createEvent(scene_rdl2::logging::ERROR_LEVEL,
        "ref_N primitive attribute is missing and cannot be computed from ref_P partials");

    staticData.sErrorMissingdPds = logEventRegistry.createEvent(scene_rdl2::logging::WARN_LEVEL,
        "dPds is not provided");

    const scene_rdl2::rdl2::SceneVariables &sceneVariables =
        shader->getSceneClass().getSceneContext()->getSceneVariables();

    asCpp(staticData.sFatalColor) =
        sceneVariables.get(scene_rdl2::rdl2::SceneVariables::sFatalColor);

    MOONRAY_FINISH_THREADSAFE_STATIC_WRITE
}

std::unique_ptr<moonray::shading::Xform>
getProjectorXform(const scene_rdl2::rdl2::SceneObject *shader,
                  const ispc::PROJECTION_Mode projectionMode,
                  const scene_rdl2::rdl2::SceneObject* projectorObject,
                  const Mat4d &projectionMatrix,
                  const ispc::PROJECTION_TransformOrder transformOrder,
                  const ispc::PROJECTION_RotationOrder rotationOrder,
                  const Vec3f& translation,
                  const Vec3f& rotation,
                  const Vec3f& scale)
{
    switch(projectionMode) {
    case ispc::PROJECTION_MODE_PROJECTOR:
        if (projectorObject != nullptr) {
            const scene_rdl2::rdl2::Node* projectorNode = projectorObject->asA<scene_rdl2::rdl2::Node>();
            return fauxstd::make_unique<moonray::shading::Xform>(shader, projectorNode,
                                                        nullptr, nullptr);
        }
        break;
    case ispc::PROJECTION_MODE_MATRIX:
        return fauxstd::make_unique<moonray::shading::Xform>(shader, projectionMatrix);
        break;
    case ispc::PROJECTION_MODE_TRS:
        Xform3d rot(scene_rdl2::math::one);
        switch (rotationOrder) {
        case ispc::ROTATION_ORDER_XYZ:
            rot *= rot.rotate(Vec3d(1.0f, 0.0f, 0.0f), deg2rad(rotation.x));
            rot *= rot.rotate(Vec3d(0.0f, 1.0f, 0.0f), deg2rad(rotation.y));
            rot *= rot.rotate(Vec3d(0.0f, 0.0f, 1.0f), deg2rad(rotation.z));
            break;
        case ispc::ROTATION_ORDER_XZY:
            rot *= rot.rotate(Vec3d(1.0f, 0.0f, 0.0f), deg2rad(rotation.x));
            rot *= rot.rotate(Vec3d(0.0f, 0.0f, 1.0f), deg2rad(rotation.z));
            rot *= rot.rotate(Vec3d(0.0f, 1.0f, 0.0f), deg2rad(rotation.y));
            break;
        case ispc::ROTATION_ORDER_YXZ:
            rot *= rot.rotate(Vec3d(0.0f, 1.0f, 0.0f), deg2rad(rotation.y));
            rot *= rot.rotate(Vec3d(1.0f, 0.0f, 0.0f), deg2rad(rotation.x));
            rot *= rot.rotate(Vec3d(0.0f, 0.0f, 1.0f), deg2rad(rotation.z));
            break;
        case ispc::ROTATION_ORDER_YZX:
            rot *= rot.rotate(Vec3d(0.0f, 1.0f, 0.0f), deg2rad(rotation.y));
            rot *= rot.rotate(Vec3d(0.0f, 0.0f, 1.0f), deg2rad(rotation.z));
            rot *= rot.rotate(Vec3d(1.0f, 0.0f, 0.0f), deg2rad(rotation.x));
            break;
        case ispc::ROTATION_ORDER_ZXY:
            rot *= rot.rotate(Vec3d(0.0f, 0.0f, 1.0f), deg2rad(rotation.z));
            rot *= rot.rotate(Vec3d(1.0f, 0.0f, 0.0f), deg2rad(rotation.x));
            rot *= rot.rotate(Vec3d(0.0f, 1.0f, 0.0f), deg2rad(rotation.y));
            break;
        case ispc::ROTATION_ORDER_ZYX:
            rot *= rot.rotate(Vec3d(0.0f, 0.0f, 1.0f), deg2rad(rotation.z));
            rot *= rot.rotate(Vec3d(0.0f, 1.0f, 0.0f), deg2rad(rotation.y));
            rot *= rot.rotate(Vec3d(1.0f, 0.0f, 0.0f), deg2rad(rotation.x));
            break;
        }

        Xform3d transform(scene_rdl2::math::one);
        switch (transformOrder) {
        case ispc::TRANSFORM_ORDER_SCALE_ROT_TRANS:
            transform *= transform.scale(scale);
            transform *= rot;
            transform *= transform.translate(translation);
            break;
        case ispc::TRANSFORM_ORDER_SCALE_TRANS_ROT:
            transform *= transform.scale(scale);
            transform *= transform.translate(translation);
            transform *= rot;
            break;
        case ispc::TRANSFORM_ORDER_ROT_SCALE_TRANS:
            transform *= rot;
            transform *= transform.scale(scale);
            transform *= transform.translate(translation);
            break;
        case ispc::TRANSFORM_ORDER_ROT_TRANS_SCALE:
            transform *= rot;
            transform *= transform.translate(translation);
            transform *= transform.scale(scale);
            break;
        case ispc::TRANSFORM_ORDER_TRANS_SCALE_ROT:
            transform *= transform.translate(translation);
            transform *= transform.scale(scale);
            transform *= rot;
            break;
        case ispc::TRANSFORM_ORDER_TRANS_ROT_SCALE:
            transform *= transform.translate(translation);
            transform *= rot;
            transform *= transform.scale(scale);
            break;
        }

        Mat4d projectionMatrix(transform);
        return fauxstd::make_unique<moonray::shading::Xform>(shader, projectionMatrix);
    }

    return nullptr;
}

} // projection
} // moonshine


