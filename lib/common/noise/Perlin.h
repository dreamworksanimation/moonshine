// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//

#pragma once

#include "Noise.h"
#include "Noise_ispc_stubs.h"
#include "Perlin_ispc_stubs.h"
#include <scene_rdl2/common/math/Vec4.h>

namespace moonshine {
namespace noise {

class Perlin : public Noise
{
public:
    explicit Perlin(const int seed = 0, 
                    const int tableSize = 2048,
                    const bool useStaticTables = false,
                    const bool fourD = false);

    // Get a pointer to the perlin noise data, required to use the 
    // library functions in ISPC during shade.
    const ispc::NOISE_Perlin* getIspcPerlin() const;

    // C++/ISPC - The public functions below have both C++ and ISPC versions

    // Returns the perlin noise for the passed in position
    float perlin3D(const scene_rdl2::math::Vec3f &pos) const;

    // Returns the perlin noise for the passed in position with 4d time component
    float perlin4D(const scene_rdl2::math::Vec4f &pos) const;

    // Returns maxLevel number of accumulated perlin 
    // noise for the passed in position
    float perlinFractal3D(const scene_rdl2::math::Vec3f &pos,
                          const float maxLevel,
                          const float persistence,
                          const float lacunarity
    ) const;

    // Returns maxLevel number of accumulated perlin 
    // noise for the passed in position with time component
    float perlinFractal4D(const scene_rdl2::math::Vec4f &pos,
                          const float maxLevel,
                          const float persistence,
                          const float lacunarity
    ) const;

private:
    // Returns dot product of the indexed noise gradient and
    // the fractional position within the lattice cell 
    float glattice3D(const int ix,
                     const int iy,
                     const int iz, 
                     const float fx, 
                     const float fy, 
                     const float fz) const;

    float glattice4D(const int ix,
                     const int iy,
                     const int iz,
                     const int iw,
                     const float fx,
                     const float fy,
                     const float fz,
                     const float fw) const;

    // Build the perlin specific noise tables in 3D or 4D
    // based on fourD argument
    void buildPerlinTables(const int tableSize, const bool fourD);

private:

    std::vector<float> mGradientsX;
    std::vector<float> mGradientsY;
    std::vector<float> mGradientsZ;
    std::vector<float> mGradientsW;

    ispc::NOISE_Perlin mIspcPerlin;
};

}
}

