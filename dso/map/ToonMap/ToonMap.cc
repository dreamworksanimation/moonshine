// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ToonMap.cc

#include "attributes.cc"
#include "ToonMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>
#include <scene_rdl2/common/math/Color.h>
#include <scene_rdl2/scene/rdl2/rdl2.h>

using namespace scene_rdl2::math;
using namespace moonray::shading;

//---------------------------------------------------------------------------
// The toon shader traces outlines and creases of a 3D model. A 3D model
// contains a set of polygons and shared edges. We draw the edges as outlines
// when one of the two polygons sharing an edge faces toward the eye while
// the other faces the opposite direction. A shared edge is an outline if
// dot((e-n0), (e-n1)) <= 0 where e is a vector from any point on the edge
// to the eye. In addition, we might want to draw other visible edges as
// creases if dot(n0, n1) <= threshold. Please see the Artistic Shading
// section in the book Fundamentals of Computer Graphics by Shirley and
// Marschner for more information on the algorithm.
//---------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(ToonMap, scene_rdl2::rdl2::Map)

public:
    ToonMap(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name);
    ~ToonMap();
    virtual void update();

private:
    static void sample(const scene_rdl2::rdl2::Map* self, moonray::shading::TLState *tls,
                       const moonray::shading::State& state, Color* sample);

    ispc::ToonMap mData;

RDL2_DSO_CLASS_END(ToonMap)

//---------------------------------------------------------------------------

namespace {

// This function computes the normals of 8 neighbors around the state.P
void
computeNeighborNormals(const moonray::shading::State& state, Vec3f (&n)[8], const float scale)
{
    const Vec3f normal = state.getN();

    const float dsdx = state.getdSdx() * scale;
    const float dsdy = state.getdSdy() * scale;
    const float dtdx = state.getdTdx() * scale;
    const float dtdy = state.getdTdy() * scale;

    const Vec3f dndx = dsdx * state.getdNds() +
                       dtdx * state.getdNdt();
    const Vec3f dndy = dsdy * state.getdNds() +
                       dtdy * state.getdNdt();

    n[0] = normalize(normal - dndx - dndy);
    n[1] = normalize(normal + dndx - dndy);
    n[2] = normalize(normal + dndx + dndy);
    n[3] = normalize(normal - dndx + dndy);

    n[4] = normalize(normal - dndx);
    n[5] = normalize(normal - dndy);
    n[6] = normalize(normal + dndx);
    n[7] = normalize(normal + dndy);
}

bool
isOutline(const moonray::shading::State& state, const Vec3f (&n)[8], const float threshold)
{
    const Vec3f e = normalize(state.getP());
    if (dot(e, n[0]) * dot(e, n[2]) <= threshold ||
        dot(e, n[1]) * dot(e, n[3]) <= threshold ||
        dot(e, n[4]) * dot(e, n[6]) <= threshold ||
        dot(e, n[5]) * dot(e, n[7]) <= threshold) {
        return true;
    }
    return false;
}

bool
isCrease(const Vec3f (&n)[8], const float threshold)
{
    if (dot(n[0], n[2]) <= threshold ||
        dot(n[1], n[3]) <= threshold ||
        dot(n[4], n[6]) <= threshold ||
        dot(n[5], n[7]) <= threshold) {
        return true;
    }
    return false;
}

} // end of namespace

//---------------------------------------------------------------------------

ToonMap::ToonMap(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mSampleFunc = ToonMap::sample;
    mSampleFuncv = (scene_rdl2::rdl2::SampleFuncv) ispc::ToonMap_getSampleFunc();
}

ToonMap::~ToonMap()
{
}

void
ToonMap::update()
{
    mRequiredAttributes.clear();

    // Get the number of poly vertices
    mRequiredAttributes.push_back(StandardAttributes::sNumPolyVertices);

    // Get the type of poly vertices
    mRequiredAttributes.push_back(StandardAttributes::sPolyVertexType);

    // Get the number of poly vertices for ispc
    mData.mNumPolyVerticesIndx = StandardAttributes::sNumPolyVertices.getIndex();

    // Get the type of poly vertices for ispc
    mData.mPolyVertexTypeIndx = StandardAttributes::sPolyVertexType.getIndex();
}

void
ToonMap::sample(const scene_rdl2::rdl2::Map* self,
                moonray::shading::TLState *tls,
                const moonray::shading::State& state,
                Color* sample)
{
    const ToonMap* me = static_cast<const ToonMap*>(self);
    const Color fillColor = evalColor(me, attrFillColor, tls, state);
    *sample = fillColor;

    // what type of data is in our poly vertex array?
    const int ptype = state.getAttribute(StandardAttributes::sPolyVertexType);

    if (ptype != StandardAttributes::POLYVERTEX_TYPE_POLYGON) {
        return;
    }

    const int numPolyVertices = state.getAttribute(StandardAttributes::sNumPolyVertices);

    if (numPolyVertices) {
        // Get the input parameters
        const Color outlineColor = evalColor(me, attrOutlineColor, tls, state);
        const Color creaseColor = evalColor(me, attrCreaseColor, tls, state);
        const float outlineThreshold = evalFloat(me, attrOutlineThreshold, tls, state);
        const float creaseThreshold = cos(evalFloat(me, attrCreaseThreshold, tls, state) * sPi / 180.f);
        const float outlineScale = evalFloat(me, attrOutlineScale, tls, state);
        const float creaseScale = evalFloat(me, attrCreaseScale, tls, state);
        Vec3f normals[8];

        // Draw outline
        switch (me->get(attrMode)) {
        case ispc::ToonMode::OUTLINE:
        {
            computeNeighborNormals(state, normals, outlineScale);
            if (isOutline(state, normals, outlineThreshold))
                *sample = outlineColor;
            break;
        }

        // Draw crease
        case ispc::ToonMode::CREASE:
        {
            computeNeighborNormals(state, normals, creaseScale);
            if (isCrease(normals, creaseThreshold))
                *sample = creaseColor;
            break;
        }

        // Draw outline and crease
        case ispc::ToonMode::BOTH:
        default:
        {
            computeNeighborNormals(state, normals, outlineScale);
            if (isOutline(state, normals, outlineThreshold))
                *sample = outlineColor;
            else {
                computeNeighborNormals(state, normals, creaseScale);
                if (isCrease(normals, creaseThreshold))
                    *sample = creaseColor;
            }
            break;
        }
        } // end of switch
    }
    return;
}

//---------------------------------------------------------------------------

