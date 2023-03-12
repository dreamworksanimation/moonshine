// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

///
/// @file attributes.cc
/// $Id$
///

#include <scene_rdl2/scene/rdl2/rdl2.h>

RDL2_DSO_ATTR_DECLARE
    // declare rdl2 attribute that geometry shader can access
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float> attrRadius;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float> attrZMin;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float> attrZMax;
    scene_rdl2::rdl2::AttributeKey<scene_rdl2::rdl2::Float> attrPhiMax;

RDL2_DSO_ATTR_DEFINE(scene_rdl2::rdl2::Geometry)
    // define rdl2 attribute type, name and default value
    attrRadius =
        sceneClass.declareAttribute<scene_rdl2::rdl2::Float>("radius", 1);
    attrZMin =
        sceneClass.declareAttribute<scene_rdl2::rdl2::Float>("zmin", -1);
    attrZMax =
        sceneClass.declareAttribute<scene_rdl2::rdl2::Float>("zmax", 1);
    attrPhiMax =
        sceneClass.declareAttribute<scene_rdl2::rdl2::Float>("phi_max", 360, { "phi max" });
    sceneClass.setMetadata(attrPhiMax, "label", "phi max");



    // grouping the attributes for Torch - the order of 
    // the attributes should be the same as how they are defined.
    sceneClass.setGroup("Quadric", attrRadius);
    sceneClass.setGroup("Quadric", attrZMin);
    sceneClass.setGroup("Quadric", attrZMax);
    sceneClass.setGroup("Quadric", attrPhiMax);

RDL2_DSO_ATTR_END

