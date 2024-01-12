// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "Perlin.isph"

// Export ispc enums and structs to c++
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(NOISE_Perlin);

//----------------------------------------------------------------------
// Simple implementation of 3D perlin lattice noise.
// Adapted from the older pdi noise library (lib/noise/ShaderNoise_perlin.cc).
// This was itself likely based on Perlin, Ken 2002 "Improving Noise."
// There is a regrettable amount of duplicated logic with the scalar code
//----------------------------------------------------------------------

// Smooth input between 0 and 1
varying float
scurve(varying float t)
{
    return t * t * (3 - 2 * t);
}

// Returns dot product of the indexed noise gradient and
// the fractional position within the lattice cell 
varying float
perlinGlattice3D(const uniform NOISE_Perlin * uniform n,
                 varying int ix,
                 varying int iy,
                 varying int iz,
                 varying float fx,
                 varying float fy,
                 varying float fz)
{
    varying int i = NOISE_index3D(n->mNoise, ix, iy, iz);
    return n->mGradientsX[i] * fx +
           n->mGradientsY[i] * fy +
           n->mGradientsZ[i] * fz;
}

// Returns single level of perlin noise for the passed in position
varying float
NOISE_perlin3D(const uniform NOISE_Perlin * uniform n,
                const varying Vec3f &pos)
{
    const varying int ix = (varying int) floor(pos.x);
    const varying int iy = (varying int) floor(pos.y);
    const varying int iz = (varying int) floor(pos.z);

    const varying float fx0 = pos.x - (varying float) ix;
    const varying float fy0 = pos.y - (varying float) iy;
    const varying float fz0 = pos.z - (varying float) iz;

    const varying float fx1 = fx0 - 1.f;
    const varying float fy1 = fy0 - 1.f;
    const varying float fz1 = fz0 - 1.f;

    const varying float wx = scurve(fx0);
    const varying float wy = scurve(fy0);
    const varying float wz = scurve(fz0);

    varying float vx0 = perlinGlattice3D(n, ix, iy, iz, fx0, fy0, fz0);
    varying float vx1 = perlinGlattice3D(n, ix + 1, iy, iz, fx1, fy0, fz0);
    varying float vy0 = lerp(vx0, vx1, wx);

    vx0 = perlinGlattice3D(n, ix, iy + 1, iz, fx0, fy1, fz0);
    vx1 = perlinGlattice3D(n, ix + 1, iy + 1, iz, fx1, fy1, fz0);
    varying float vy1 = lerp(vx0, vx1, wx);

    varying float vz0 = lerp(vy0, vy1, wy);

    vx0 = perlinGlattice3D(n, ix, iy, iz + 1, fx0, fy0, fz1);
    vx1 = perlinGlattice3D(n, ix + 1, iy, iz + 1, fx1, fy0, fz1);
    vy0 = lerp(vx0, vx1, wx);

    vx0 = perlinGlattice3D(n, ix, iy + 1, iz + 1, fx0, fy1, fz1);
    vx1 = perlinGlattice3D(n, ix + 1, iy + 1, iz + 1, fx1, fy1, fz1);
    vy1 = lerp(vx0, vx1, wx);

    varying float vz1 = lerp(vy0, vy1, wy);

    return lerp(vz0, vz1, wz);
}

varying float
perlinGlattice4D(const uniform NOISE_Perlin * uniform n,
                 varying int ix,
                 varying int iy,
                 varying int iz,
                 varying int iw,
                 varying float fx,
                 varying float fy,
                 varying float fz,
                 varying float fw)
{
    varying int i = NOISE_index4D(n->mNoise, ix, iy, iz, iw);
    return n->mGradientsX[i] * fx +
           n->mGradientsY[i] * fy +
           n->mGradientsZ[i] * fz +
           n->mGradientsW[i] * fw;
}

varying float
NOISE_perlin4D(const uniform NOISE_Perlin * uniform n,
                const varying Vec4f &pos)
{
    const varying int ix = (varying int) floor(pos.x);
    const varying int iy = (varying int) floor(pos.y);
    const varying int iz = (varying int) floor(pos.z);
    const varying int iw = (varying int) floor(pos.w);

    const varying float fx0 = pos.x - (varying float) ix;
    const varying float fy0 = pos.y - (varying float) iy;
    const varying float fz0 = pos.z - (varying float) iz;
    const varying float fw0 = pos.w - (varying float) iw;

    const varying float fx1 = fx0 - 1.f;
    const varying float fy1 = fy0 - 1.f;
    const varying float fz1 = fz0 - 1.f;
    const varying float fw1 = fw0 - 1.f;

    const varying float wx = scurve(fx0);
    const varying float wy = scurve(fy0);
    const varying float wz = scurve(fz0);
    const varying float ww = scurve(fw0);

    varying float vx0 = perlinGlattice4D(n, ix, iy, iz, iw, fx0, fy0, fz0, fw0);
    varying float vx1 = perlinGlattice4D(n, ix + 1, iy, iz, iw, fx1, fy0, fz0, fw0);
    varying float vy0 = lerp(vx0, vx1, wx);

    vx0 = perlinGlattice4D(n, ix, iy + 1, iz, iw, fx0, fy1, fz0, fw0);
    vx1 = perlinGlattice4D(n, ix + 1, iy + 1, iz, iw, fx1, fy1, fz0, fw0);
    varying float vy1 = lerp(vx0, vx1, wx);

    varying float vz0 = lerp(vy0, vy1, wy);

    vx0 = perlinGlattice4D(n, ix, iy, iz + 1, iw, fx0, fy0, fz1, fw0);
    vx1 = perlinGlattice4D(n, ix + 1, iy, iz + 1, iw, fx1, fy0, fz1, fw0);
    vy0 = lerp(vx0, vx1, wx);

    vx0 = perlinGlattice4D(n, ix, iy + 1, iz + 1, iw, fx0, fy1, fz1, fw0);
    vx1 = perlinGlattice4D(n, ix + 1, iy + 1, iz + 1, iw, fx1, fy1, fz1, fw0);
    vy1 = lerp(vx0, vx1, wx);

    varying float vz1 = lerp(vy0, vy1, wy);

    varying float vw0 = lerp(vz0, vz1, wz);

	vx0 = perlinGlattice4D(n, ix, iy, iz, iw + 1, fx0, fy0, fz0, fw1);
	vx1 = perlinGlattice4D(n, ix + 1, iy, iz, iw + 1, fx1, fy0, fz0, fw1);
	vy0 = lerp(vx0, vx1, wx);

	vx0 = perlinGlattice4D(n, ix, iy + 1, iz, iw + 1, fx0, fy1, fz0, fw1);
	vx1 = perlinGlattice4D(n, ix + 1, iy + 1, iz, iw + 1, fx1, fy1, fz0, fw1);
	vy1 = lerp(vx0, vx1, wx);

	vz0 = lerp(vy0, vy1, wy);

	vx0 = perlinGlattice4D(n, ix, iy, iz + 1, iw + 1, fx0, fy0, fz1, fw1);
	vx1 = perlinGlattice4D(n, ix + 1, iy, iz + 1, iw + 1, fx1, fy0, fz1, fw1);
	vy0 = lerp(vx0, vx1, wx);

	vx0 = perlinGlattice4D(n, ix, iy + 1, iz + 1, iw + 1, fx0, fy1, fz1, fw1);
	vx1 = perlinGlattice4D(n, ix + 1, iy + 1, iz + 1, iw + 1, fx1, fy1, fz1, fw1);
	vy1 = lerp(vx0, vx1, wx);

	vz1 = lerp(vy0, vy1, wy);

	float vw1 = lerp(vz0, vz1, wz);

	return lerp(vw0, vw1, ww);
}

// Returns maxLevel number of accumulated perlin noise 
// for the passed in position.   
// lacunarity - acts as a multiplier on the noise frequency
// for each octave.
// persistence - acts as a multiplier on the noise amplitude
// for each octace.
// Together lacunarity and persistence affect the apparent
// roughness of the noise.
varying float 
NOISE_perlinFractal3D(const uniform NOISE_Perlin * uniform n,
                       const varying Vec3f &pos,
                       const varying float maxLevel,
                       const varying float persistence,
                       const varying float lacunarity)
{
    Vec3f ppos = pos;
    varying float noise = 0.f;
    varying float scale = 1.f;

    varying int numLevels = (varying int) ceil(maxLevel);
    for (varying int i = 0; i < numLevels; ++i) {
        if (i + 1 == numLevels) {
            scale = scale * (1.f - ((varying float) numLevels - maxLevel));
        }
        noise = noise + NOISE_perlin3D(n, ppos) * scale;
        scale *= persistence;
        ppos = ppos * lacunarity;
    }
    return noise;
}

varying float NOISE_perlinFractal4D(const uniform NOISE_Perlin * uniform n,
                                     const varying Vec4f &pos,
                                     const varying float maxLevel,
                                     const varying float persistence,
                                     const varying float lacunarity)
{
    Vec4f ppos = pos;
    varying float noise = 0.f;
    varying float scale = 1.f;

    varying int numLevels = (varying int) ceil(maxLevel);
    for (varying int i = 0; i < numLevels; ++i) {
        if (i + 1 == numLevels) {
            scale = scale * (1.f - ((varying float) numLevels - maxLevel));
        }
        noise = noise + NOISE_perlin4D(n, ppos) * scale;
        scale *= persistence;
        ppos = ppos * lacunarity;
    }
    return noise;
}


