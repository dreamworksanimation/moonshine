// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

///
/// @file attributes.cc
/// $Id$
///

#include <scene_rdl2/scene/rdl2/rdl2.h>

RDL2_DSO_ATTR_DECLARE
    // declare rdl2 attribute that geometry shader can access
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Vec3f> attrSize;

RDL2_DSO_ATTR_DEFINE(scene_rdl2::rdl2::Geometry)
    // define rdl2 attribute type, name and default value
    attrSize =
        sceneClass.declareAttribute<scene_rdl2::rdl2::Vec3f>("size", scene_rdl2::rdl2::Vec3f(1,1,1));
    sceneClass.setMetadata(attrSize, "comment", "The size of the box");
    sceneClass.setGroup("Box", attrSize);

RDL2_DSO_ATTR_END

