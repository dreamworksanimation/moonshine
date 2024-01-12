// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file ToonMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

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

struct ToonMap
{
    uniform int mPolyVertexTypeIndx;
    uniform int mNumPolyVerticesIndx;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(ToonMap);

// should match geom::POLYVERTEX_TYPE_POLYGON::POLYVERTEX_TYPE_POLYGON
enum PolyVertexType {
    POLYVERTEX_TYPE_UNKNOWN = 0,    // likely a program error
    POLYVERTEX_TYPE_POLYGON,        // polyvertices define a triangle, quad, etc...
    POLYVERTEX_TYPE_CUBIC_SPLINE,   // polyvertices define a cubic spline span
    POLYVERTEX_TYPE_LINE            // polyvertices define a line segment
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(PolyVertexType);

enum ToonMode {
    OUTLINE = 0,
    CREASE = 1,
    BOTH = 2
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(ToonMode);

// This function computes the normals of 8 neighbors around the state.P
inline void
computeNeighborNormals(const varying State &state, 
                       varying Vec3f (&n)[8],
                       const varying float scale)
{
    const varying Vec3f normal = state.mN;

    const varying float dsdx = getdSdx(state) * scale;
    const varying float dsdy = getdSdy(state) * scale;
    const varying float dtdx = getdTdx(state) * scale;
    const varying float dtdy = getdTdy(state) * scale;

    const varying Vec3f dndx = dsdx * getdNds(state) +
                               dtdx * getdNdt(state);
    const varying Vec3f dndy = dsdy * getdNds(state) +
                               dtdy * getdNdt(state);

    n[0] = normalize(normal - dndx - dndy);
    n[1] = normalize(normal + dndx - dndy);
    n[2] = normalize(normal + dndx + dndy);
    n[3] = normalize(normal - dndx + dndy);

    n[4] = normalize(normal - dndx);
    n[5] = normalize(normal - dndy);
    n[6] = normalize(normal + dndx);
    n[7] = normalize(normal + dndy);
}

inline varying bool
isOutline(const varying State &state, 
          const varying Vec3f (&n)[8],
          const varying float threshold)
{
    const Vec3f e = normalize(state.mP);
    if (dot(e, n[0]) * dot(e, n[2]) <= threshold ||
        dot(e, n[1]) * dot(e, n[3]) <= threshold ||
        dot(e, n[4]) * dot(e, n[6]) <= threshold ||
        dot(e, n[5]) * dot(e, n[7]) <= threshold) {
        return true;
    }
    return false;
}

bool
isCrease(const varying Vec3f (&n)[8], const varying float threshold)
{
    if (dot(n[0], n[2]) <= threshold ||
        dot(n[1], n[3]) <= threshold ||
        dot(n[4], n[6]) <= threshold ||
        dot(n[5], n[7]) <= threshold) {
        return true;
    }
    return false;
}

static Color
sample(const uniform Map *uniform map,
       uniform ShadingTLState * uniform tls,
       const varying State &state)
{
    const uniform ToonMap * uniform me = MAP_GET_ISPC_PTR(ToonMap, map);
    const Color fillColor = evalAttrFillColor(map, tls, state);
    Color result = fillColor;

    // what type of data is in our poly vertex array?
    const varying int ptype = getIntAttribute(tls, state, me->mPolyVertexTypeIndx);

    if (ptype != POLYVERTEX_TYPE_POLYGON) {
        return result;
    }

    const varying int numPolyVertices = getIntAttribute(tls, state, me->mNumPolyVerticesIndx);

    if (numPolyVertices) {
        // Get the input parameters
        const Color outlineColor = evalAttrOutlineColor(map, tls, state);
        const Color creaseColor = evalAttrCreaseColor(map, tls, state);
        const float outlineThreshold = evalAttrOutlineThreshold(map, tls, state);
        const float creaseThreshold = cos(evalAttrCreaseThreshold(map, tls, state) * sPi / 180.f);
        const float outlineScale = evalAttrOutlineScale(map, tls, state);
        const float creaseScale = evalAttrCreaseScale(map, tls, state);
        varying Vec3f normals[8];

        switch (getAttrMode(map)) {
        case OUTLINE:
        {
            computeNeighborNormals(state, normals, outlineScale);
            if (isOutline(state, normals, outlineThreshold))
                result = outlineColor;
            break;
        }
        case CREASE:
        {
            computeNeighborNormals(state, normals, creaseScale);
            if (isCrease(normals, creaseThreshold))
                result = creaseColor;
            break;
        }
        case BOTH:
        default:
        {
            computeNeighborNormals(state, normals, outlineScale);
            if (isOutline(state, normals, outlineThreshold))
                result = outlineColor;
            else {
                computeNeighborNormals(state, normals, creaseScale);
                if (isCrease(normals, creaseThreshold))
                    result = creaseColor;
            }
            break;
        }
        } // end of switch
    }
    return result;
}

DEFINE_MAP_SHADER(ToonMap, sample)

