// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

///
/// @file attributes.cc
/// $Id$
///

#include <scene_rdl2/scene/rdl2/rdl2.h>

RDL2_DSO_ATTR_DECLARE
    // declare rdl2 attribute that geometry shader can access
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Int> attrTestInt;

RDL2_DSO_ATTR_DEFINE(scene_rdl2::rdl2::Geometry)
    // define rdl2 attribute type, name and default value
    attrTestInt = sceneClass.declareAttribute<scene_rdl2::rdl2::Int>("test_int", 5);

    // grouping the attributes for Torch - the order of 
    // the attributes should be the same as how they are defined.
    sceneClass.setGroup("TemplateGroup", attrTestInt);

RDL2_DSO_ATTR_END

