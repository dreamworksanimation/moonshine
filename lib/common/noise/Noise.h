// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//

#pragma once

#pragma warning disable 1711 // Warnings about assignemnt to statically allocated data

#include "Noise_ispc_stubs.h"
#include <scene_rdl2/common/math/Vec3.h>
#include <scene_rdl2/common/math/Xform.h>
#include <mutex>

// Forward declaration
namespace scene_rdl2 {
namespace util {
    class Random;
}
}

namespace moonshine {
namespace noise {

class Noise
{
public:
    explicit Noise(const int seed,
                   const int tableSize = 2048,
                   const bool useStaticTables = false);

    virtual ~Noise();

    // Get a pointer to noise data, required to use the 
    // library functions in ISPC during shade.
    const ispc::NOISE_Noise* getIspc() const;

    // C++/ISPC - Functions below have both C++ and ISPC versions
   
    // Builds transformation matrix for noise shaders in the
    // same way the Moonlight shaders did it.
    scene_rdl2::math::Xform3f orderedCompose(const scene_rdl2::math::Vec3f &translation, 
                                        const scene_rdl2::math::Vec3f &rotation, 
                                        const scene_rdl2::math::Vec3f &scale, 
                                        const int transformationOrder, 
                                        const int rotationOrder
    ) const;

protected:
    // Retrievesa a random value from the permutation table
    int perm(const int i) const;

    // Retrieves a random value from the permutation table
    // calculated recursively from the three inputs
    int index3D(const int ix, const int iy, const int iz) const;

    // Retrieves a random value from the permutation table
    // calculated recursively from the four inputs
    int index4D(const int ix, const int iy, const int iz, const int iw) const;

    // Random number generator used generate noise tables
    // for this class and it's sub classes.
    scene_rdl2::util::Random *mRandomNumberGenerator;

    std::vector<int> mPermutationTable;

    // An instance of the ispc Noise structure which
    // gets populated in c++ functions
    ispc::NOISE_Noise mIspc;

    // Static data
    static bool sNoiseIsDataInitialized;
    static scene_rdl2::util::Random sNoiseRandom;
    static std::mutex sNoiseInitDataMutex;
    static std::vector<int> sNoisePermutationTable;

private:
    // Build the permutation tables from which to retrive
    // pre-cached pseudo random numbers.
    virtual void buildTables(const int tableSize);

    // Build the static permutation tables from which to retrive
    // pre-cached pseudo random numbers.
    virtual void buildStaticTables(const int tableSize);
};

}
}

