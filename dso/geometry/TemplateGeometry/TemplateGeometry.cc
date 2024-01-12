// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

///
/// @file TemplateGeometry.cc
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

RDL2_DSO_CLASS_BEGIN(TemplateGeometry, scene_rdl2::rdl2::Geometry)

public:
    RDL2_DSO_DEFAULT_CTOR(TemplateGeometry)
    geom::Procedural* createProcedural() const;
    void destroyProcedural() const;    

RDL2_DSO_CLASS_END(TemplateGeometry)

//------------------------------------------------------------------------------

namespace moonshine {

class TemplateProcedural : public ProceduralLeaf
{
public:
    // constructor can be freely extended but should always pass in State to
    // construct base Procedural class
    TemplateProcedural(const moonray::geom::State& state, int testInt) :
        ProceduralLeaf(state), mTestInt(testInt) {}

    void generate(const GenerateContext& generateContext,
            const XformSamples& parent2render)
    {
        // Implement this method to procedurally generate primitives
    }
private:
    // replace with rdl2 attributes that are needed as procedural parameters
    int mTestInt;
};

} // namespace moonshine

//------------------------------------------------------------------------------

geom::Procedural* TemplateGeometry::createProcedural() const
{
    moonray::geom::State state;
    // Pass in rdl2 attribute declared in attributes.cc
    int testInt = get(attrTestInt);
    return new moonshine::TemplateProcedural(state, testInt);
}

void TemplateGeometry::destroyProcedural() const
{
    delete mProcedural;
}
