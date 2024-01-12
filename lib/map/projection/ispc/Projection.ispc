// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file Projection.ispc

#include "Projection.isph"

#include <moonray/rendering/shading/ispc/ShaderDataAux.isph>

ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(PROJECTION_StaticData);
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(PROJECTION_TransformOrder);
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(PROJECTION_RotationOrder);
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(PROJECTION_Mode);

