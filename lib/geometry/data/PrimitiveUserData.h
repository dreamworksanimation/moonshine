// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

///

#pragma once

#include <scene_rdl2/scene/rdl2/Geometry.h>
#include <scene_rdl2/scene/rdl2/UserData.h>
#include <moonray/rendering/bvh/shading/PrimitiveAttribute.h>

namespace moonshine {
namespace geometry {

void
processMeshUserData(const scene_rdl2::rdl2::SceneObjectVector& arbitraryData,
                    const std::vector<int>& partIndices,
                    int meshPartIndex,
                    bool useFirstFrame, bool useSecondFrame,
                    moonray::shading::PrimitiveAttributeTable& primitiveAttributeTable);

void
processUserData(const scene_rdl2::rdl2::SceneObjectVector& arbitraryData,
                const std::vector<std::string> * const partList,
                const std::string& partName,
                bool useFirstFrame, bool useSecondFrame,
                moonray::shading::PrimitiveAttributeTable& primitiveAttributeTable);

} // namespace geometry 
} // namespace moonshine 


