// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file CurvatureMap.cc

#include "attributes.cc"
#include "CurvatureMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>

using namespace scene_rdl2::math;
using namespace moonray::shading;

//---------------------------------------------------------------------------
// The curvature map shader draws an edge line whose line width depends on
// the angle between normals of adjacent faces sharing this edge line.
// Therefore the shader shows the convexity and concavity of a 3D model.
// Given a sampling point, we define a quad whose vertices are around this
// point. We also define the other quad by extending the vertices along their
// respective normals. Then We compare the areas between the first quad and
// the extended one. The sampling point sits on a convex surface if the
// extended area is larger than the original area, or a concave surface
// otherwise.
//---------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(CurvatureMap, scene_rdl2::rdl2::Map)

public:
    CurvatureMap(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name);
    ~CurvatureMap();
    virtual void update();

private:
    static void sample(const scene_rdl2::rdl2::Map* self, moonray::shading::TLState *tls,
                       const moonray::shading::State& state, Color* sample);

    ispc::CurvatureMap mData;

RDL2_DSO_CLASS_END(CurvatureMap)

//---------------------------------------------------------------------------

namespace {
float
areaQuad(const Vec3f (&verts)[4])
{
    //Area of a parallelogram
    Vec3f s1 = verts[1] - verts[0];
    Vec3f s2 = verts[3] - verts[0];

    float area = length(cross(s1, s2));

    return area;
}

// This function computes the coordinates and normals of four corners around the state.P
void
computeQuadCorners(const moonray::shading::State& state, Vec3f (&p)[4], Vec3f (&n)[4])
{
    const Vec3f position = state.getP();
    const Vec3f normal = state.getN();

    const float dsdx = state.getdSdx();
    const float dtdy = state.getdTdy();

    const Vec3f dpds = 0.5f * dsdx * state.getdPds();
    const Vec3f dpdt = 0.5f * dtdy * state.getdPdt();

    // four corner positions
    p[0] = position - dpds - dpdt;
    p[1] = position + dpds - dpdt;
    p[2] = position + dpds + dpdt;
    p[3] = position - dpds + dpdt;

    // same for normals
    const Vec3f dnds = 0.5f * dsdx * state.getdNds();
    const Vec3f dndt = 0.5f * dtdy * state.getdNdt();

    n[0] = normalize(normal - dnds - dndt);
    n[1] = normalize(normal + dnds - dndt);
    n[2] = normalize(normal + dnds + dndt);
    n[3] = normalize(normal - dnds + dndt);
}

// This function compares preA to postA. If preA < postA, convexC is positive and concaveC is zero.
// Otherwise, convexC is zero and concaveC is positive.
void
computeCurvatures(float power, float scale,
                  float preA, float postA,
                  float &convexC, float &concaveC)
{
    float ratio = 0.0;
    if (preA > sEpsilon)
        ratio = postA / preA;

    convexC = ratio - 1.0;
    concaveC = 1.0 - ratio;

    convexC *= scale;
    concaveC *= scale;

    convexC = clamp(convexC, 0.0f, 1.0f);
    concaveC = clamp(concaveC, 0.0f, 1.0f);

    if (power != 1) {
        convexC = pow(convexC, power);
        concaveC = pow(concaveC, power);
    }
}
} // namespace

//---------------------------------------------------------------------------

CurvatureMap::CurvatureMap(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mSampleFunc = CurvatureMap::sample;
    mSampleFuncv = (scene_rdl2::rdl2::SampleFuncv) ispc::CurvatureMap_getSampleFunc();
}

CurvatureMap::~CurvatureMap()
{
}

void
CurvatureMap::update()
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
CurvatureMap::sample(const scene_rdl2::rdl2::Map* self,
                     moonray::shading::TLState *tls,
                     const moonray::shading::State& state,
                     Color* sample)
{
    const CurvatureMap* me = static_cast<const CurvatureMap*>(self);

    // what type of data is in our poly vertex array?
    const int ptype = state.getAttribute(StandardAttributes::sPolyVertexType);

    if (ptype != StandardAttributes::POLYVERTEX_TYPE_POLYGON) {
        *sample = sWhite;
        return;
    }

    const int numPolyVertices = state.getAttribute(StandardAttributes::sNumPolyVertices);

    if (numPolyVertices <= 0) {
        *sample = sBlack;
    } else {
        const bool isInvert = me->get(attrInvert);
        const float power = me->get(attrPower);
        const float scale = me->get(attrScale);

        Vec3f quadPositions[4], quadNormals[4];
        computeQuadCorners(state, quadPositions, quadNormals);

        // extend the poly vertices along their respective normals
        Vec3f quadPositionsExtended[4];
        for (int i = 0; i < 4; i++) {
            quadPositionsExtended[i] = quadPositions[i] + quadNormals[i];
        }

        // area of a parallelogram before and after
        float quadArea = areaQuad(quadPositions);
        float quadExtendedArea = areaQuad(quadPositionsExtended);

        float convexC = 0.0, concaveC = 0.0;
        computeCurvatures(power, scale, quadArea, quadExtendedArea, convexC, concaveC);

        if (isInvert == true) {
            concaveC = 1.0 - concaveC;
            convexC = 1.0 - convexC;
        }

        switch (me->get(attrMode)) {
        case ispc::CONVEX:
            *sample = Color(convexC);
            break;
        case ispc::CONCAVE:
            *sample = Color(concaveC);
            break;
        case ispc::COMPOSITE:
            *sample = Color(((concaveC - convexC) * 0.5) + 0.5);
            break;
        case ispc::ALL:
        default:
            *sample = Color(concaveC, convexC, ((concaveC - convexC) * 0.5) + 0.5);
            break;
        }
    }
    return;
}

//---------------------------------------------------------------------------

