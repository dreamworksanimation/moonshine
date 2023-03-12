// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file WireframeMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

// should match geom::StandardAttributes::MAX_NUM_POLYVERTICES
#define MAX_POLY_VERTICES 16


struct WireframeMap
{
    uniform int mPolyVertexTypeIndx;
    uniform int mNumPolyVerticesIndx;
    uniform int mPolyVertices[MAX_POLY_VERTICES];
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(WireframeMap);

// should match geom::POLYVERTEX_TYPE_POLYGON::POLYVERTEX_TYPE_POLYGON
enum PolyVertexType {
    POLYVERTEX_TYPE_UNKNOWN = 0,  // likely a program error
    POLYVERTEX_TYPE_POLYGON,      // polyvertices define a triangle, quad, etc...
    POLYVERTEX_TYPE_CUBIC_SPLINE, // polyvertices define a cubic spline span
    POLYVERTEX_TYPE_LINE          // polyvertices define a line segment
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(PolyVertexType);

static Color
sample(const uniform Map *uniform map,
       uniform ShadingTLState * uniform tls,
       const varying State &state)
{
    const uniform WireframeMap * uniform me = MAP_GET_ISPC_PTR(WireframeMap, map);
    // return lineColor if the intersection point is on a polygon edge, fillColor if not
    const uniform Color fillColor = getAttrFillColor(map);
    const uniform Color lineColor = getAttrLineColor(map);
    Color result = fillColor;

    // what type of data is in our poly vertex array?
    const varying int ptype = getIntAttribute(tls, state, me->mPolyVertexTypeIndx);
    if (ptype == POLYVERTEX_TYPE_UNKNOWN) {
        // well if you don't know, then neither do we.
        return fillColor;
    }

    if (ptype == POLYVERTEX_TYPE_CUBIC_SPLINE || ptype == POLYVERTEX_TYPE_LINE) {
        // curves are basically all edge.  the main issue is curve thickness
        // that will not in general match the desired raster pixel width.
        // for now, a hit on a curve is a hit on the edge.
        return lineColor;
    }

    MNRY_ASSERT(ptype == POLYVERTEX_TYPE_POLYGON);

    const uniform bool isRaster = getAttrRaster(map);

    const uniform float lineWidth = getAttrLineWidth(map);
    const uniform float lineThickness = lineWidth * 0.5;

    const varying int numPolyVertices = getIntAttribute(tls, state, me->mNumPolyVerticesIndx);

    if (numPolyVertices) {
        // estimate pixel width in render space at point P
        //
        //        pwX
        //    +--------+ P + dpdx
        //    |       /
        //    |      /
        //    |     /
        //    |    /
        //    |   /
        //    |  /
        //    | /
        //    +
        //    P
        const varying Vec3f dpdx = getdPdx(state);
        const varying Vec3f dpdy = getdPdy(state);

        const varying Vec3f I = normalize(state.mP);

        const varying float pwXSq = lengthSqr(dpdx - dot(I, dpdx) * I);
        const varying float pwYSq = lengthSqr(dpdy - dot(I, dpdy) * I);

        // we want to consider just half the estimated pixel width for this
        // polygon, neighboring polygons will account for the other half
        //    (pwX / 2) * (pwX / 2) = pwXSq * .25
        //    (pwY / 2) * (PwY / 2) = pwYSq * .25
        // and then we want to average these values
        // to get our final pwSq estimate that we'll use for comparisons
        //    (pwXSq * .25 + pwYSq * .25) * .5

        const varying float pw = (sqrt(pwXSq) + sqrt(pwYSq)) * .5f;
        // When "lineWidth" is specified in "world space",
        // renderSpaceLineThickness is equal to lineWidth * .75
        // We use the constant .75 to match the results of Arnold's wireframe shader
        varying float renderSpaceLineThickness = lineThickness + lineThickness * .5;
        // When "lineWidth" is in raster space (num of pixels), then
        // renderSpaceLineThickness is equal to pw * lineWidth.
        if (isRaster) renderSpaceLineThickness = pw * lineWidth;
        varying float renderSpaceLineThicknessSq = renderSpaceLineThickness * renderSpaceLineThickness;
        
        for (varying int i1 = 0; i1 < numPolyVertices; ++i1) {
            varying int i0 = (i1 == 0)? numPolyVertices - 1 : i1 - 1;

            const varying Vec3f p0 = getVec3fAttribute(tls, state, me->mPolyVertices[i0]);
            const varying Vec3f p1 = getVec3fAttribute(tls, state, me->mPolyVertices[i1]);
            
            const varying Vec3f edge = p1 - p0;
            const varying Vec3f dir = state.mP - p0;

            const varying float areaASq = lengthSqr(cross(edge, dir));
            // "B" is the triangle (p1, p, p0), which is half of "A"
            //      areaA = areaB * 2
            //      areaAsq = areaBSq * 4
            //
            // "d" is the height of B, and more importantly, the
            // closest distance from the intersection point p, to the edge.
            //    areaB   = d * length(edge) / 2
            //    areaBSq = dSq * lengthSqr(edge) / 4
            //    areaASq = dSq * lengthSqr(edge)
            //
            // in the case where p is on the edge, d must be within line thickness
            // or d < renderSpaceLineThickness
            if (areaASq < renderSpaceLineThicknessSq * lengthSqr(edge)) {
                return lineColor; // all done
            }
        }
    }
    return result;
}

DEFINE_MAP_SHADER(WireframeMap, sample)

