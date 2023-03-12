// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

///
/// @file SphereGeometry.cc
/// $Id$
///

#include "attributes.cc"
#include <moonray/rendering/geom/Api.h>
#include <moonray/rendering/geom/ProceduralLeaf.h>
#include <moonray/rendering/geom/Types.h>
#include <moonray/rendering/shading/Shading.h>
#include <scene_rdl2/scene/rdl2/rdl2.h>

using namespace moonray;
using namespace moonray::geom;
using namespace moonray::shading;

RDL2_DSO_CLASS_BEGIN(SphereGeometry, scene_rdl2::rdl2::Geometry)

public:
    RDL2_DSO_DEFAULT_CTOR(SphereGeometry)
    moonray::geom::Procedural* createProcedural() const;
    void destroyProcedural() const;    

RDL2_DSO_CLASS_END(SphereGeometry)

//------------------------------------------------------------------------------

namespace moonshine {

class SphereProcedural : public ProceduralLeaf
{
public:
    // constructor can be freely extended but should always pass in State to
    // construct base Procedural class
    explicit SphereProcedural(const moonray::geom::State& state) : ProceduralLeaf(state) {}

    void generate(const GenerateContext& generateContext,
            const XformSamples& parent2render)
    {     
        // use rdl::Geometry and rdl::Layer for layer assignment id query
        const scene_rdl2::rdl2::Geometry* rdlGeometry = generateContext.getRdlGeometry();
        const scene_rdl2::rdl2::Layer *rdlLayer = generateContext.getRdlLayer();
        // if part is not assigned with material, simply skip generate it
        int layerAssignmentId;
        if (!getAssignmentId(rdlLayer, rdlGeometry, "", layerAssignmentId)) {
            return;
        }
        const SphereGeometry* pSphereGeom = 
            static_cast<const SphereGeometry*>(rdlGeometry);
        float radius = pSphereGeom->get(attrRadius);
        float zmin = pSphereGeom->get(attrZMin);
        float zmax = pSphereGeom->get(attrZMax);
        float phiMax = pSphereGeom->get(attrPhiMax);
        bool singleSided = pSphereGeom->getSideType() > 0;
        bool reverseNormals = rdlGeometry->getReverseNormals();

        std::unique_ptr<Sphere> sphere = createSphere(radius,
            LayerAssignmentId(layerAssignmentId));
        sphere->setClippingRange(zmin, zmax, phiMax);
        sphere->setIsSingleSided(singleSided);
        sphere->setIsNormalReversed(reverseNormals);

        // may need to convert the primitive to instance to handle
        // rotation motion blur
        std::unique_ptr<Primitive> p = convertForMotionBlur(
            generateContext, std::move(sphere), parent2render.size() > 1);
        // hand the instantiated primitive to renderer
        addPrimitive(std::move(p),
            generateContext.getMotionBlurParams(), parent2render);
    }
private:
};

} // namespace moonshine

//------------------------------------------------------------------------------

moonray::geom::Procedural* SphereGeometry::createProcedural() const
{
    moonray::geom::State state;
    return new moonshine::SphereProcedural(state);
}

void SphereGeometry::destroyProcedural() const
{
    delete mProcedural;
}
