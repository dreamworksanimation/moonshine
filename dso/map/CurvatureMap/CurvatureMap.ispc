// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file CurvatureMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

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

struct CurvatureMap
{
    uniform int mPolyVertexTypeIndx;
    uniform int mNumPolyVerticesIndx;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(CurvatureMap);

// should match geom::POLYVERTEX_TYPE_POLYGON::POLYVERTEX_TYPE_POLYGON
enum PolyVertexType {
    POLYVERTEX_TYPE_UNKNOWN = 0,    // likely a program error
    POLYVERTEX_TYPE_POLYGON,        // polyvertices define a triangle, quad, etc...
    POLYVERTEX_TYPE_CUBIC_SPLINE,   // polyvertices define a cubic spline span
    POLYVERTEX_TYPE_LINE            // polyvertices define a line segment
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(PolyVertexType);

enum CurvatureMode {
    CONVEX = 0,
    CONCAVE = 1,
    COMPOSITE = 2,
    ALL = 3
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(CurvatureMode);

inline varying float
areaQuad(const varying Vec3f (&verts)[4])
{
    //Area of a parallelogram
    varying Vec3f s1 = verts[1] - verts[0];
    varying Vec3f s2 = verts[3] - verts[0];

    varying float area = length(cross(s1, s2));

    return area;
}

// This function computes the coordinates and normals of four corners around the state.P
inline void
computeQuadCorners(const varying State &state, 
                   varying Vec3f (&p)[4], 
                   varying Vec3f (&n)[4])
{
    const varying Vec3f position = state.mP;
    const varying Vec3f normal = state.mN;

    const varying float dsdx = getdSdx(state);
    const varying float dtdy = getdTdy(state);

    const varying Vec3f dpds = 0.5 * dsdx * getdPds(state);
    const varying Vec3f dpdt = 0.5 * dtdy * getdPdt(state);

    // four corner positions
    p[0] = position - dpds - dpdt;
    p[1] = position + dpds - dpdt;
    p[2] = position + dpds + dpdt;
    p[3] = position - dpds + dpdt;

    // same for normals
    const varying Vec3f dnds = 0.5 * dsdx * getdNds(state);
    const varying Vec3f dndt = 0.5 * dtdy * getdNdt(state);
    
    n[0] = normalize(normal - dnds - dndt);
    n[1] = normalize(normal + dnds - dndt);
    n[2] = normalize(normal + dnds + dndt);
    n[3] = normalize(normal - dnds + dndt);
}

// This function compares preA to postA. If preA < postA, convexC is positive and concaveC is zero.
// Otherwise, convexC is zero and concaveC is positive.
inline void
computeCurvatures(varying float power, varying float scale,
                  varying float preA, varying float postA, 
                  varying float &convexC, varying float &concaveC)
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

static Color
sample(const uniform Map *uniform map,
       uniform ShadingTLState * uniform tls,
       const varying State &state)
{
    const uniform CurvatureMap * uniform me = MAP_GET_ISPC_PTR(CurvatureMap, map);
    Color result;

    // what type of data is in our poly vertex array?
    const varying int ptype = getIntAttribute(tls, state, me->mPolyVertexTypeIndx);

    if (ptype != POLYVERTEX_TYPE_POLYGON) {
        result = sWhite;
        return result;
    }

    const varying int numPolyVertices = getIntAttribute(tls, state, me->mNumPolyVerticesIndx);

    if (numPolyVertices <= 0) {
        result = sBlack;
    } else {
        const uniform bool isInvert = getAttrInvert(map);
        const uniform float power = getAttrPower(map);
        const uniform float scale = getAttrScale(map);
        
        varying Vec3f quadPositions[4], quadNormals[4];
        computeQuadCorners(state, quadPositions, quadNormals);
        
        // extend the poly vertices along their respective normals
        varying Vec3f quadPositionsExtended[4];
        for (varying int i = 0; i < 4; i++) {
            quadPositionsExtended[i] = quadPositions[i] + quadNormals[i];
        }
        
        // area of a parallelogram before and after
        varying float quadArea = areaQuad(quadPositions);
        varying float quadExtendedArea = areaQuad(quadPositionsExtended);
        
        varying float convexC = 0.0, concaveC = 0.0;
        computeCurvatures(power, scale, quadArea, quadExtendedArea, convexC, concaveC);
        
        if (isInvert == true) {
            concaveC = 1.0 - concaveC;
            convexC = 1.0 - convexC;
        }
        
        switch (getAttrMode(map)) {
        case CONVEX:
            result = Color_ctor(convexC);
            break;
        case CONCAVE:
            result = Color_ctor(concaveC);
            break;
        case COMPOSITE:
            result = Color_ctor(((concaveC - convexC) * 0.5) + 0.5);
            break;
        case ALL:
        default:
            result = Color_ctor(concaveC, convexC, ((concaveC - convexC) * 0.5) + 0.5);
            break;
        }
    }
    return result;
}

DEFINE_MAP_SHADER(CurvatureMap, sample)

