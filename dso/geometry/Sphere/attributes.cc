// Copyright 2023-2024 DreamWorks Animation LLC
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
    attrRadius = sceneClass.declareAttribute<scene_rdl2::rdl2::Float>("radius", 1);
    sceneClass.setMetadata(attrRadius, "comment",
            "The radius of the sphere.  The sphere will clip if the radius exceeds the "
            "magnitude of either the zmin or zmax values.");
    sceneClass.setGroup("Sphere", attrRadius);

    attrZMin = sceneClass.declareAttribute<scene_rdl2::rdl2::Float>("zmin", -1);
    sceneClass.setMetadata(attrZMin, "comment",
            "The minimum extent of sphere on the z axis expressed in object space and "
            "independent of the radius.  The sphere will clip if the radius exceeds the "
            "magnitude of this parameter.");
    sceneClass.setGroup("Sphere", attrZMin);

    attrZMax = sceneClass.declareAttribute<scene_rdl2::rdl2::Float>("zmax", 1);
    sceneClass.setMetadata(attrZMax, "comment",
            "The maximum extent of sphere on the z axis expressed in object space and "
            "independent of the radius.  The sphere will clip if the radius exceeds the "
            "magnitude of this parameter.");
    sceneClass.setGroup("Sphere", attrZMax);

    attrPhiMax = sceneClass.declareAttribute<scene_rdl2::rdl2::Float>("phi_max", 360, { "phi max" });
    sceneClass.setMetadata(attrPhiMax, "label", "phi max");
    sceneClass.setMetadata(attrPhiMax, "comment",
            "The arc of the sphere surface around the z axis expressed in degrees.");
    sceneClass.setGroup("Sphere", attrPhiMax);

RDL2_DSO_ATTR_END

