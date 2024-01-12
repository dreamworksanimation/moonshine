// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

///
/// @file BoxGeometry.cc
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
using namespace scene_rdl2::math;
using namespace scene_rdl2::rdl2;

RDL2_DSO_CLASS_BEGIN(BoxGeometry, Geometry)

public:
    RDL2_DSO_DEFAULT_CTOR(BoxGeometry)
    moonray::geom::Procedural* createProcedural() const;
    void destroyProcedural() const;

RDL2_DSO_CLASS_END(BoxGeometry)

//------------------------------------------------------------------------------

namespace moonshine {

class BoxProcedural : public ProceduralLeaf
{
public:
    // constructor can be freely extended but should always pass in State to
    // construct base Procedural class
    explicit BoxProcedural(const moonray::geom::State& state) : ProceduralLeaf(state) {}

    void generate(const GenerateContext& generateContext,
            const XformSamples& parent2render)
    {
        // use rdl::Geometry and rdl::Layer for layer assignment id query
        const Geometry* rdlGeometry = generateContext.getRdlGeometry();
        const Layer *rdlLayer = generateContext.getRdlLayer();
        // if part is not assigned with material, simply skip generate it
        int layerAssignmentId;
        if (!getAssignmentId(rdlLayer, rdlGeometry, "", layerAssignmentId)) {
            return;
        }
        const BoxGeometry* pBoxGeom =
            static_cast<const BoxGeometry*>(rdlGeometry);
        Vec3f size = pBoxGeom->get(attrSize);
        bool singleSided = pBoxGeom->getSideType() > 0;
        bool reverseNormals = rdlGeometry->getReverseNormals();

        std::unique_ptr<Box> box = createBox(size.x, size.y, size.z,
            LayerAssignmentId(layerAssignmentId));
        box->setIsSingleSided(singleSided);
        box->setIsNormalReversed(reverseNormals);

        // may need to convert the primitive to instance to handle
        // rotation motion blur
        std::unique_ptr<Primitive> p = convertForMotionBlur(
            generateContext, std::move(box), parent2render.size() > 1);
        // hand the instantiated primitive to renderer
        addPrimitive(std::move(p),
            generateContext.getMotionBlurParams(), parent2render);
    }
private:
};

} // namespace moonshine

//------------------------------------------------------------------------------

moonray::geom::Procedural* BoxGeometry::createProcedural() const
{
    moonray::geom::State state;
    return new moonshine::BoxProcedural(state);
}

void BoxGeometry::destroyProcedural() const
{
    delete mProcedural;
}
