// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//

#include "Simplex.h"
#include "Constants.h"

// Code based on public domain work by Stefan Gustavson
// https://github.com/stegu/perlin-noise/blob/master/LICENSE.md
//
// From Stefan Gustavson,
// "C implementation of Perlin simplex noise with rotating
// gradients and analytic derivative.
//
// This is an implementation of Perlin "simplex noise" over three dimensions
// (x,y,z) and four dimensions (x,y,z,w). One extra parameter 'angle' rotates the
// underlying gradients of the grid, which gives a swirling, flow-like
// motion. The derivative is returned, to make it possible to do pseudo-
// advection and implement "flow noise", as presented by Ken Perlin and
// Fabrice Neyret at Siggraph 2001.
//
// When not animated and presented in one octave only, this noise
// looks exactly the same as the plain version of simplex noise.
// It's nothing magical by itself, although the extra animation
// parameter 't' is useful. Fun stuff starts to happen when you
// do fractal sums of several octaves, with different rotation speeds
// and an advection of smaller scales by larger scales (or even the
// other way around it you feel adventurous).
//
// The gradient rotations that can be performed by this noise function
// and the true analytic derivatives are required to do flow noise.
// You can't do it properly with regular Perlin noise."
//

namespace moonshine {
namespace noise {

using namespace scene_rdl2::math;

Simplex::Simplex(const int seed,
                 const bool fourD) :
                 Noise(seed)
{
    // Assign the base class mIspc member to the SHADER_NoiseBase
    // member of the NoisePerlin ispc structure
    mIspcSimplex.mNoise = &mIspc;
}

// Helper function to compute rotated gradients
Vec3f
Simplex::gradRot3D(const int hash,
                   const float sin_t,
                   const float cos_t) const
{
    int h = hash & 15;
    const Vec3f gU = Vec3f(
        sSimplexGradients3DU[h][0], 
        sSimplexGradients3DU[h][1], 
        sSimplexGradients3DU[h][2]
    );
    const Vec3f gV = Vec3f(
        sSimplexGradients3DV[h][0], 
        sSimplexGradients3DV[h][1], 
        sSimplexGradients3DV[h][2]
    );
    return cos_t * gU + sin_t * gV;
}

float
Simplex::simplex3D(const Vec3f &pos,
                   const float angle,
                   Vec3f &noiseDerivatives) const
{
    // Noise contributions from the four simplex corners
    float ns[4];

    // Return value
    float noise;

    int hashArray[4];
    float t0[4], t2[4], t4[4], dTmp[4], gDot[4];
    Vec3f gRot[4];

    // Sine and cosine for the gradient rotation angle
    float sin_t, cos_t;
    sincos(angle, &sin_t, &cos_t);

    // Skew the input space to determine which simplex cell we're in
    float s = (pos.x + pos.y + pos.z) * sF3;
    int i0 = (int)scene_rdl2::math::floor(pos.x + s);
    int j0 = (int)scene_rdl2::math::floor(pos.y + s);
    int k0 = (int)scene_rdl2::math::floor(pos.z + s);

    // Unskew the cell origin back to (x,y,z) space
    float t = (float)(i0 + j0 + k0) * sG3; 
    // The x,y,z distances from the cell origin
    Vec3f distances[4];
    distances[0].x = pos.x - (i0 - t);
    distances[0].y = pos.y - (j0 - t);
    distances[0].z = pos.z - (k0 - t);

    // For the 3D case, the simplex shape is a slightly irregular 
    // tetrahedron.  Determine which simplex we are in.
    
    // Offsets for second and third corner of 
    // simplex in (i,j,k) coords
    int i1, j1, k1;
    int i2, j2, k2;
    if(distances[0].x >= distances[0].y) {
        if(distances[0].y >= distances[0].z) {
            i1=1; j1=0; k1=0; i2=1; j2=1; k2=0;
        } else if(distances[0].x >= distances[0].z) {
            i1=1; j1=0; k1=0; i2=1; j2=0; k2=1;
        } else {
            i1=0; j1=0; k1=1; i2=1; j2=0; k2=1;
        }
    }
    else {
        if(distances[0].y < distances[0].z) {
            i1=0; j1=0; k1=1; i2=0; j2=1; k2=1;
        } else if(distances[0].x < distances[0].z) {
            i1=0; j1=1; k1=0; i2=0; j2=1; k2=1;
        } else {
            i1=0; j1=1; k1=0; i2=1; j2=1; k2=0;
        }
    }

    // Offsets for second corner in (x,y,z) coords
    distances[1].x = distances[0].x - i1 + sG3; 
    distances[1].y = distances[0].y - j1 + sG3; 
    distances[1].z = distances[0].z - k1 + sG3; 


    // Offsets for third corner in (x,y,z) coords
    distances[2].x = distances[0].x - i2 + 2.0f * sG3;
    distances[2].y = distances[0].y - j2 + 2.0f * sG3;
    distances[2].z = distances[0].z - k2 + 2.0f * sG3;

    // Offsets for last corner in (x,y,z) coords
    distances[3].x = distances[0].x - 1.0f + 3.0f * sG3;
    distances[3].y = distances[0].y - 1.0f + 3.0f * sG3;
    distances[3].z = distances[0].z - 1.0f + 3.0f * sG3;

    // Wrap the integer indices at 256, to avoid indexing perm[] out of bounds
    int ii = i0 % 256;
    int jj = j0 % 256;
    int kk = k0 % 256;

    hashArray[0] = index3D(ii + 0,  jj + 0,  kk + 0);
    hashArray[1] = index3D(ii + i1, jj + j1, kk + k1);
    hashArray[2] = index3D(ii + i2, jj + j2, kk + k2);
    hashArray[3] = index3D(ii + 1,  jj + 1,  kk + 1);

    // Calculate the contribution from the four corners
    noiseDerivatives = 0.0f;
    for (int p = 0; p < 4; p++) {
        t0[p] = 0.5f - dot(distances[p], distances[p]);
        t2[p] = t0[p] * t0[p];
        t4[p] = t2[p] * t2[p];
        
        gRot[p] = gradRot3D(hashArray[p], sin_t, cos_t);

        if (t0[p] < 0.0) {
            ns[p] = 0.0f;
            t0[p] = 0.0f;
            t2[p] = 0.0f;
            t4[p] = 0.0f;
            gDot[p] = 0.0f;
        }
        else {
            gDot[p] =  dot(gRot[p], distances[p]);
            ns[p] = t4[p] * gDot[p];
        }

        // Derivatives
        dTmp[p] = t2[p] * t0[p] * gDot[p];
        noiseDerivatives += dTmp[p] * distances[p];
    }

    // Add contributions from each corner to get the final noise value.
    // The result is scaled to return values in the range [-1,1]
    noise = 28.0f * (ns[0] + ns[1] + ns[2] + ns[3]);

    // Derivatives
    noiseDerivatives *= -8.0f;
    noiseDerivatives += 
        t4[0] * gRot[0] +
        t4[1] * gRot[1] +
        t4[2] * gRot[2] +
        t4[3] * gRot[3];
    noiseDerivatives *= 28.0f;

    return noise;
}
        
float
Simplex::simplexFractal3D(const Vec3f &pos,
                          float angle,
                          const float advection,
                          const float maxLevel,
                          const float persistence,
                          const float lacunarity) const
{
    Vec3f ppos = pos;
    Vec3f noiseDerivatives;
    Vec3f advect = 0.f;
    float weight = 1.f;
    float noise = 0.f;

    const int numLevels = scene_rdl2::math::ceil(maxLevel);
    for (int i = 0; i < numLevels; ++i) {

        if (i + 1 == numLevels) {
            weight *= 1.f - ((float) numLevels - maxLevel);
        }

        noise += simplex3D(ppos, angle, noiseDerivatives) * weight;

        weight *= persistence;
        ppos *= lacunarity;
        ppos += advect;
        angle *= lacunarity;

        if (i == 0) {
            advect = -advection * noiseDerivatives;
        }
        else {
            advect -= advection * noiseDerivatives * weight;
        }
    }
    return noise;
}

Vec4f
Simplex::gradRot4D(const int hash,
                   const float sin_t,
                   const float cos_t) const
{
    int h = hash & 31;
    const Vec4f gU = Vec4f(
        sSimplexGradients4DU[h][0], 
        sSimplexGradients4DU[h][1], 
        sSimplexGradients4DU[h][2],
        sSimplexGradients4DU[h][3]
    );
    const Vec4f gV = Vec4f(
        sSimplexGradients4DV[h][0], 
        sSimplexGradients4DV[h][1], 
        sSimplexGradients4DV[h][2],
        sSimplexGradients4DV[h][3]
    );
    Vec4f result = cos_t * gU + sin_t * gV;
    result.w = sSimplexGradients4DU[h][3];
    return result;
}

float
Simplex::simplex4D(const Vec4f &pos,
                   const float angle,
                   Vec4f &noiseDerivatives) const
{
    // Noise contributions from the five(4d) simplex corners
    float ns[5];

    // Return value
    float noise;

    int hashArray[5];
    float t0[5], t2[5], t4[5], dTmp[5], gDot[5];
    Vec4f gRot[5];

    // Sine and cosine for the gradient rotation angle
    float sin_t, cos_t;
    scene_rdl2::math::sincos(angle, &sin_t, &cos_t);

    // Skew the input space to determine which simplex cell we're in
    float s = (pos.x + pos.y + pos.z + pos.w) * sF4;
    int i0 = (int)scene_rdl2::math::floor(pos.x + s);
    int j0 = (int)scene_rdl2::math::floor(pos.y + s);
    int k0 = (int)scene_rdl2::math::floor(pos.z + s);
    int l0 = (int)scene_rdl2::math::floor(pos.w + s);

    // Unskew the cell origin back to (x,y,z) space
    float t = (float)(i0 + j0 + k0 + l0) * sG4; 
    // The x,y,z distances from the cell origin
    Vec4f distances[5];
    distances[0].x = pos.x - (i0 - t);
    distances[0].y = pos.y - (j0 - t);
    distances[0].z = pos.z - (k0 - t);
    distances[0].w = pos.w - (l0 - t);

    // From Stefan Gustavson,
    // "For the 4D case, the simplex is a 4D shape I won't even try to describe.
    // To find out which of the 24 possible simplices we're in, we need to
    // determine the magnitude ordering of x0, y0, z0 and w0.
    // Six pair-wise comparisons are performed between each possible pair
    // of the four coordinates, and the results are used to rank the numbers."
    int rankx = 0;
    int ranky = 0;
    int rankz = 0;
    int rankw = 0;
    if(distances[0].x > distances[0].y) rankx++; else ranky++;
    if(distances[0].x > distances[0].z) rankx++; else rankz++;
    if(distances[0].x > distances[0].w) rankx++; else rankw++;
    if(distances[0].y > distances[0].z) ranky++; else rankz++;
    if(distances[0].y > distances[0].w) ranky++; else rankw++;
    if(distances[0].z > distances[0].w) rankz++; else rankw++;
    int i1, j1, k1, l1; // The integer offsets for the second simplex corner
    int i2, j2, k2, l2; // The integer offsets for the third simplex corner
    int i3, j3, k3, l3; // The integer offsets for the fourth simplex corner
    // [rankx, ranky, rankz, rankw] is a 4-vector with the numbers 0, 1, 2 and 3
    // in some order. We use a thresholding to set the coordinates in turn.
	// Rank 3 denotes the largest coordinate.
    i1 = (rankx >= 3) ? 1 : 0;
    j1 = (ranky >= 3) ? 1 : 0;
    k1 = (rankz >= 3) ? 1 : 0;
    l1 = (rankw >= 3) ? 1 : 0;
    // Rank 2 denotes the second largest coordinate.
    i2 = (rankx >= 2) ? 1 : 0;
    j2 = (ranky >= 2) ? 1 : 0;
    k2 = (rankz >= 2) ? 1 : 0;
    l2 = (rankw >= 2) ? 1 : 0;
    // Rank 1 denotes the second smallest coordinate.
    i3 = (rankx >= 1) ? 1 : 0;
    j3 = (ranky >= 1) ? 1 : 0;
    k3 = (rankz >= 1) ? 1 : 0;
    l3 = (rankw >= 1) ? 1 : 0;
    // The fifth corner has all coordinate offsets = 1, so no need to compute that.
    distances[1].x = distances[0].x - i1 + sG4; // Offsets for second corner in (x,y,z,w) coords
    distances[1].y = distances[0].y - j1 + sG4;
    distances[1].z = distances[0].z - k1 + sG4;
    distances[1].w = distances[0].w - l1 + sG4;
    distances[2].x = distances[0].x - i2 + 2.0 * sG4; // Offsets for third corner in (x,y,z,w) coords
    distances[2].y = distances[0].y - j2 + 2.0 * sG4;
    distances[2].z = distances[0].z - k2 + 2.0 * sG4;
    distances[2].w = distances[0].w - l2 + 2.0 * sG4;
    distances[3].x = distances[0].x - i3 + 3.0 * sG4; // Offsets for fourth corner in (x,y,z,w) coords
    distances[3].y = distances[0].y - j3 + 3.0 * sG4;
    distances[3].z = distances[0].z - k3 + 3.0 * sG4;
    distances[3].w = distances[0].w - l3 + 3.0 * sG4;
    distances[4].x = distances[0].x - 1.0 + 4.0 * sG4; // Offsets for last corner in (x,y,z,w) coords
    distances[4].y = distances[0].y - 1.0 + 4.0 * sG4;
    distances[4].z = distances[0].z - 1.0 + 4.0 * sG4;
    distances[4].w = distances[0].w - 1.0 + 4.0 * sG4;
    // Work out the hashed gradient indices of the five simplex corners
    int ii = i0 % 256;
    int jj = j0 % 256;
    int kk = k0 % 256;
    int ll = l0 % 256;

    hashArray[0] = index4D(ii + 0,  jj + 0,  kk + 0, ll + 0) % 32;
    hashArray[1] = index4D(ii + i1, jj + j1, kk + k1, ll + l1) % 32;
    hashArray[2] = index4D(ii + i2, jj + j2, kk + k2, ll + l2) % 32;
    hashArray[3] = index4D(ii + i3, jj + j3, kk + k3, ll + l3) % 32;
    hashArray[4] = index4D(ii + 1,  jj + 1,  kk + 1, ll + 1) % 32;

    // Calculate the contribution from the four corners
    noiseDerivatives = Vec4f(0.0f, 0.0f, 0.0f, 0.0f);
    for (int p = 0; p < 5; p++) {
        t0[p] = 0.5f - dot(distances[p], distances[p]);
        t2[p] = t0[p] * t0[p];
        t4[p] = t2[p] * t2[p];
        
        gRot[p] = gradRot4D(hashArray[p], sin_t, cos_t);

        if (t0[p] < 0.0) {
            ns[p] = 0.0f;
            t0[p] = 0.0f;
            t2[p] = 0.0f;
            t4[p] = 0.0f;
            gDot[p] = 0.0f;
        }
        else {
            gDot[p] =  dot(gRot[p], distances[p]);
            ns[p] = t4[p] * gDot[p];
        }

        // Derivatives
        dTmp[p] = t2[p] * t0[p] * gDot[p];
        noiseDerivatives += dTmp[p] * distances[p];
    }

    //  Add contributions from each corner to get the final noise value.
    // The result is scaled to return values in the range [-1,1]
    noise = 27.0f * (ns[0] + ns[1] + ns[2] + ns[3] + ns[4]);

    // Derivatives
    noiseDerivatives *= -8.0f;
    noiseDerivatives += 
        t4[0] * gRot[0] +
        t4[1] * gRot[1] +
        t4[2] * gRot[2] +
        t4[3] * gRot[3] +
        t4[4] * gRot[4];
    noiseDerivatives *= 27.0f;

    return noise;
}

float
Simplex::simplexFractal4D(const Vec4f &pos,
                          float angle,
                          const float advection,
                          const float maxLevel,
                          const float persistence,
                          const float lacunarity) const
{
    Vec4f ppos = pos;
    Vec4f noiseDerivatives;
    Vec4f advect = Vec4f(0.0f, 0.0f, 0.0f, 0.0f);
    float weight = 1.f;
    float noise = 0.f;

    const int numLevels = scene_rdl2::math::ceil(maxLevel);
    for (int i = 0; i < numLevels; ++i) {

        if (i + 1 == numLevels) {
            weight *= 1.f - ((float) numLevels - maxLevel);
        }

        noise += simplex4D(ppos, angle, noiseDerivatives) * weight;

        weight *= persistence;
        ppos *= lacunarity;
        ppos += advect;
        angle *= lacunarity;

        if (i == 0) {
            advect = -advection * noiseDerivatives;
        }
        else {
            advect -= advection * noiseDerivatives * weight;
        }
    }
    return noise;
}

const ispc::NOISE_Simplex*
Simplex::getIspcSimplex() const
{
    return &mIspcSimplex;
}


}
}

