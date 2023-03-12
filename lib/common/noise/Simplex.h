// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//

#pragma once

#include "Noise.h"
#include "Noise_ispc_stubs.h"
#include "Simplex_ispc_stubs.h"
#include <scene_rdl2/common/math/Vec4.h>

namespace moonshine {
namespace noise {

class Simplex : public Noise
{
public:
    explicit Simplex(const int seed,
                     const bool fourD);

    // Get a pointer to the noise data, required to use the 
    // library functions in ISPC during shade.
    const ispc::NOISE_Simplex* getIspcSimplex() const;

    // C++/ISPC - The public functions below have both C++ and ISPC versions

    // Returns the perlin simplex noise for the passed in position.
    // Also returns the analytic derivatives in noiseDerivatives.
    float simplex3D(const scene_rdl2::math::Vec3f &pos,
                    const float angle,
                    scene_rdl2::math::Vec3f &noiseDerivatives) const;

    // Returns the perlin simplex noise for the passed in position with time component.
    // Also returns the analytic derivatives in noiseDerivatives.
    float simplex4D(const scene_rdl2::math::Vec4f &pos,
                    const float angle,
                    scene_rdl2::math::Vec4f &noiseDerivatives) const;

    // Returns maxLevel number of accumulated perlin simplex noise 
    // for the passed in 3D position
    float simplexFractal3D(const scene_rdl2::math::Vec3f &pos,
                           float angle,
                           const float advection,
                           const float maxLevel,
                           const float persistence,
                           const float lacunarity) const;

    // Returns maxLevel number of accumulated perlin simplex noise 
    // for the passed in 4D position
    float simplexFractal4D(const scene_rdl2::math::Vec4f &pos,
                           float angle,
                           const float advection,
                           const float maxLevel,
                           const float persistence,
                           const float lacunarity) const;

private:
    // Helper functions to compute rotated gradients
    scene_rdl2::math::Vec3f gradRot3D(const int hash,
                                 const float sin_t,
                                 const float cos_t) const;

    scene_rdl2::math::Vec4f gradRot4D(const int hash,
                                 const float sin_t,
                                 const float cos_t) const;

protected:
    ispc::NOISE_Simplex mIspcSimplex;
};

}
}

