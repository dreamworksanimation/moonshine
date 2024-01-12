// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//

#include "Perlin.h"
#include <scene_rdl2/render/util/Random.h>

namespace moonshine {
namespace noise {

using namespace scene_rdl2::math;

//----------------------------------------------------------------------
// Simple implementation of 3D perlin lattice noise.
// Adapted from the older pdi noise library (lib/noise/noise_perlin.cc).
// This was itself likely based on Perlin, Ken 2002 "Improving Noise."
// There is a regrettable amount of duplicated logic with the vector code
//----------------------------------------------------------------------
namespace {
float
scurve(float t)
{
    return t * t * (3 - 2 * t);
}

} // end anonymouse namespace

Perlin::Perlin(const int seed,
               const int tableSize,
               const bool useStaticTables,
               const bool fourD) :
               Noise(seed, tableSize, useStaticTables)
{
    // Assign the base class mIspc member to the NoiseBase
    // member of the Noise ispc structure
    mIspcPerlin.mNoise = &mIspc;

    buildPerlinTables(tableSize, fourD);

    mIspcPerlin.mGradientsX = mGradientsX.data();
    mIspcPerlin.mGradientsY = mGradientsY.data();
    mIspcPerlin.mGradientsZ = mGradientsZ.data();
    if (fourD) {
        mIspcPerlin.mGradientsW = mGradientsW.data();
    }
}

void
Perlin::buildPerlinTables(const int tableSize, const bool fourD)
{
    mGradientsX.reserve(tableSize);
    mGradientsY.reserve(tableSize);
    mGradientsZ.reserve(tableSize);

    if (!fourD) {
        for (int i = 0; i < tableSize; ++i) {
            Vec3f v;
            do {
                v.x = mRandomNumberGenerator->getNextFloat() * 2 - 1;
                v.y = mRandomNumberGenerator->getNextFloat() * 2 - 1;
                v.z = mRandomNumberGenerator->getNextFloat() * 2 - 1;
            } while (dot(v, v) > 1);
            v = normalize(v);
            mGradientsX[i] = v.x;
            mGradientsY[i] = v.y;
            mGradientsZ[i] = v.z;
        }
    } else {
        mGradientsW.reserve(tableSize);
        for (int i = 0; i < tableSize; ++i) {
            Vec4f v;
            do {
                v.x = mRandomNumberGenerator->getNextFloat() * 2 - 1;
                v.y = mRandomNumberGenerator->getNextFloat() * 2 - 1;
                v.z = mRandomNumberGenerator->getNextFloat() * 2 - 1;
                v.w = mRandomNumberGenerator->getNextFloat() * 2 - 1;
            } while (dot(v, v) > 1);
            v = normalize(v);
            mGradientsX[i] = v.x;
            mGradientsY[i] = v.y;
            mGradientsZ[i] = v.z;
            mGradientsW[i] = v.w;
        }
    }
}

// Returns dot product of the indexed noise gradient and
// the fractional position within the lattice cell 
float
Perlin::glattice3D(const int ix,
                   const int iy,
                   const int iz, 
                   const float fx,
                   const float fy,
                   const float fz) const
{
    int i = index3D(ix, iy, iz);
    return mIspcPerlin.mGradientsX[i] * fx +
           mIspcPerlin.mGradientsY[i] * fy +
           mIspcPerlin.mGradientsZ[i] * fz;
}

// Returns single level of perlin noise for the passed in position
float
Perlin::perlin3D(const Vec3f &pos) const
{
    const int ix = (int) floor(pos.x);
    const int iy = (int) floor(pos.y);
    const int iz = (int) floor(pos.z);

    const float fx0 = pos.x - (float) ix;
    const float fy0 = pos.y - (float) iy;
    const float fz0 = pos.z - (float) iz;

    const float fx1 = fx0 - 1.f;
    const float fy1 = fy0 - 1.f;
    const float fz1 = fz0 - 1.f;

    const float wx = scurve(fx0);
    const float wy = scurve(fy0);
    const float wz = scurve(fz0);

    float vx0 = glattice3D(ix, iy, iz, fx0, fy0, fz0);
    float vx1 = glattice3D(ix + 1, iy, iz, fx1, fy0, fz0);
    float vy0 = scene_rdl2::math::lerp(vx0, vx1, wx);

    vx0 = glattice3D(ix, iy + 1, iz, fx0, fy1, fz0);
    vx1 = glattice3D(ix + 1, iy + 1, iz, fx1, fy1, fz0);
    float vy1 = scene_rdl2::math::lerp(vx0, vx1, wx);

    float vz0 = scene_rdl2::math::lerp(vy0, vy1, wy);

    vx0 = glattice3D(ix, iy, iz + 1, fx0, fy0, fz1);
    vx1 = glattice3D(ix + 1, iy, iz + 1, fx1, fy0, fz1);
    vy0 = scene_rdl2::math::lerp(vx0, vx1, wx);

    vx0 = glattice3D(ix, iy + 1, iz + 1, fx0, fy1, fz1);
    vx1 = glattice3D(ix + 1, iy + 1, iz + 1, fx1, fy1, fz1);
    vy1 = scene_rdl2::math::lerp(vx0, vx1, wx);

    float vz1 = scene_rdl2::math::lerp(vy0, vy1, wy);

    return scene_rdl2::math::lerp(vz0, vz1, wz);
}

// Returns maxLevel number of accumulated perlin noise 
// for the passed in position.   
// lacunarity - acts as a multiplier on the noise frequency
// for each octave.
// persistence - acts as a multiplier on the noise amplitude
// for each octace.
// Together lacunarity and persistence affect the apparent
// roughness of the noise.
float
Perlin::glattice4D(const int ix,
                   const int iy,
                   const int iz,
                   const int iw,
                   const float fx,
                   const float fy,
                   const float fz,
                   const float fw) const
{
    int i = index4D(ix, iy, iz, iw);
    return mIspcPerlin.mGradientsX[i] * fx +
           mIspcPerlin.mGradientsY[i] * fy +
           mIspcPerlin.mGradientsZ[i] * fz +
           mIspcPerlin.mGradientsW[i] * fw;
}

float
Perlin::perlin4D(const Vec4f &pos) const
{
    const int ix = (int) floor(pos.x);
    const int iy = (int) floor(pos.y);
    const int iz = (int) floor(pos.z);
    const int iw = (int) floor(pos.w);

    const float fx0 = pos.x - (float) ix;
    const float fy0 = pos.y - (float) iy;
    const float fz0 = pos.z - (float) iz;
    const float fw0 = pos.w - (float) iw;

    const float fx1 = fx0 - 1.f;
    const float fy1 = fy0 - 1.f;
    const float fz1 = fz0 - 1.f;
    const float fw1 = fw0 - 1.f;

    const float wx = scurve(fx0);
    const float wy = scurve(fy0);
    const float wz = scurve(fz0);
    const float ww = scurve(fw0);

    float vx0 = glattice4D(ix, iy, iz, iw, fx0, fy0, fz0, fw0);
    float vx1 = glattice4D(ix + 1, iy, iz, iw, fx1, fy0, fz0, fw0);
    float vy0 = scene_rdl2::math::lerp(vx0, vx1, wx);

    vx0 = glattice4D(ix, iy + 1, iz, iw, fx0, fy1, fz0, fw0);
    vx1 = glattice4D(ix + 1, iy + 1, iz, iw, fx1, fy1, fz0, fw0);
    float vy1 = scene_rdl2::math::lerp(vx0, vx1, wx);

    float vz0 = scene_rdl2::math::lerp(vy0, vy1, wy);

    vx0 = glattice4D(ix, iy, iz + 1, iw, fx0, fy0, fz1, fw0);
    vx1 = glattice4D(ix + 1, iy, iz + 1, iw, fx1, fy0, fz1, fw0);
    vy0 = scene_rdl2::math::lerp(vx0, vx1, wx);

    vx0 = glattice4D(ix, iy + 1, iz + 1, iw, fx0, fy1, fz1, fw0);
    vx1 = glattice4D(ix + 1, iy + 1, iz + 1, iw, fx1, fy1, fz1, fw0);
    vy1 = scene_rdl2::math::lerp(vx0, vx1, wx);

    float vz1 = scene_rdl2::math::lerp(vy0, vy1, wy);

    float vw0 = scene_rdl2::math::lerp(vz0, vz1, wz);

	vx0 = glattice4D(ix, iy, iz, iw + 1, fx0, fy0, fz0, fw1);
	vx1 = glattice4D(ix + 1, iy, iz, iw + 1, fx1, fy0, fz0, fw1);
	vy0 = scene_rdl2::math::lerp(vx0, vx1, wx);

	vx0 = glattice4D(ix, iy + 1, iz, iw + 1, fx0, fy1, fz0, fw1);
	vx1 = glattice4D(ix + 1, iy + 1, iz, iw + 1, fx1, fy1, fz0, fw1);
	vy1 = scene_rdl2::math::lerp(vx0, vx1, wx);

	vz0 = scene_rdl2::math::lerp(vy0, vy1, wy);

	vx0 = glattice4D(ix, iy, iz + 1, iw + 1, fx0, fy0, fz1, fw1);
	vx1 = glattice4D(ix + 1, iy, iz + 1, iw + 1, fx1, fy0, fz1, fw1);
	vy0 = scene_rdl2::math::lerp(vx0, vx1, wx);

	vx0 = glattice4D(ix, iy + 1, iz + 1, iw + 1, fx0, fy1, fz1, fw1);
	vx1 = glattice4D(ix + 1, iy + 1, iz + 1, iw + 1, fx1, fy1, fz1, fw1);
	vy1 = scene_rdl2::math::lerp(vx0, vx1, wx);

	vz1 = scene_rdl2::math::lerp(vy0, vy1, wy);

	float vw1 = scene_rdl2::math::lerp(vz0, vz1, wz);

	return scene_rdl2::math::lerp(vw0, vw1, ww);
}

float
Perlin::perlinFractal3D(const Vec3f &pos,
                        const float maxLevel,
                        const float persistence,
                        const float lacunarity) const
{
    Vec3f ppos = pos;
    float scale = 1.f;
    float noise = 0.f;

    const int numLevels = ceil(maxLevel);
    for (int i = 0; i < numLevels; ++i) {
        if (i + 1 == numLevels) {
            scale *= 1.f - ((float) numLevels - maxLevel);
        }
        noise += perlin3D(ppos) * scale;
        scale *= persistence;
        ppos *= lacunarity;
    }
    return noise;
}

float
Perlin::perlinFractal4D(const Vec4f &pos,
                        const float maxLevel,
                        const float persistence,
                        const float lacunarity) const
{
    Vec4f ppos = pos;
    float scale = 1.f;
    float noise = 0.f;

    const int numLevels = ceil(maxLevel);
    for (int i = 0; i < numLevels; ++i) {
        if (i + 1 == numLevels) {
            scale *= 1.f - ((float) numLevels - maxLevel);
        }
        noise += perlin4D(ppos) * scale;
        scale *= persistence;
        ppos *= lacunarity;
    }
    return noise;
}

const ispc::NOISE_Perlin*
Perlin::getIspcPerlin() const
{
    return &mIspcPerlin;
}


}
}

