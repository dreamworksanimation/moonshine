// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

///
/// @file Blending.h
/// $Id$
///

#pragma once

#include <moonshine/material/dwabase/Blending_ispc_stubs.h>

#include <moonray/rendering/shading/MaterialApi.h>
#include <scene_rdl2/scene/rdl2/rdl2.h>
#include <scene_rdl2/common/math/ColorSpace.h>
#include <scene_rdl2/common/math/MathUtil.h>
#include <moonshine/material/dwabase/DwaBase.h>
#include <moonshine/material/dwabase/DwaBaseLayerable.h>
#include <moonshine/material/glitter/Glitter.h>
#include <scene_rdl2/common/math/Color.h>

namespace moonshine {
namespace dwabase {

//---------------------------------------------------------------------------
// Functions used in update()

// Attempt to cast material to DwaBaseLayerable and gather func info for ISPC
const DwaBaseLayerable*
registerLayerable(const scene_rdl2::rdl2::SceneObject* sceneObject, ispc::SubMtlData& subMtlData);

const DwaBaseLayerable*
registerHairLayerable(const scene_rdl2::rdl2::SceneObject* sceneObject, ispc::SubMtlData& subMtlData);

void setUniformParameters(const ispc::DwaBaseUniformParameters &uParams,
                          ispc::DwaBaseParameters &params);

void blendUniformParameters(const ispc::DwaBaseUniformParameters &uParams0,
                            const ispc::DwaBaseUniformParameters &uParams1,
                            ispc::DwaBaseUniformParameters &uParams,
                            int fallbackSpecularModel,
                            int fallbackToonSpecularModel,
                            int fallbackOuterSpecularModel,
                            bool fallbackOuterSpecularUseBending,
                            int fallbackBSSRDF,
                            bool fallbackThinGeometry);

//---------------------------------------------------------------------------
// Functions used in shade()

float blendPresence(moonray::shading::TLState *tls,
                    const moonray::shading::State& state,
                    const DwaBaseLayerable* layerable0,
                    const DwaBaseLayerable* layerable1,
                    float mask);

float blendRefractiveIndex(moonray::shading::TLState *tls,
                           const moonray::shading::State& state,
                           const DwaBaseLayerable* layerable0,
                           const DwaBaseLayerable* layerable1,
                           float mask);

scene_rdl2::math::Vec3f blendSubsurfaceNormal(moonray::shading::TLState *tls,
                                              const moonray::shading::State& state,
                                              const DwaBaseLayerable* layerable0,
                                              const DwaBaseLayerable* layerable1,
                                              float mask);

bool blendParameters(moonray::shading::TLState *tls,
                     const moonray::shading::State& state,
                     const bool castsCaustics,
                     ispc::DwaBaseParameters &params,
                     const ispc::DwaBaseUniformParameters &uParams,
                     ispc::BlendColorSpace colorSpace,
                     const glitter::Glitter* glitterPtr,
                     intptr_t evalSubsurfaceNormal,
                     bool subMtl0HasGlitter,
                     bool subMtl1HasGlitter,
                     const DwaBaseLayerable* layerable0,
                     const DwaBaseLayerable* layerable1,
                     float mask);

bool blendHairParameters(moonray::shading::TLState *tls,
                         const moonray::shading::State& state,
                         const bool castsCaustics,
                         ispc::DwaBaseParameters &params,
                         const ispc::DwaBaseUniformParameters &uParams,
                         ispc::BlendColorSpace colorSpace,
                         intptr_t evalSubsurfaceNormal,
                         const DwaBaseLayerable* layerable0,
                         const DwaBaseLayerable* layerable1,
                         const DwaBaseLayerable* me,
                         scene_rdl2::logging::LogEvent errorMismatchedFresnelType,
                         float mask);
} // namespace dwabase
} // namespace moonshine

