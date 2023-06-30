// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//

#include "Noise.h"
#include <scene_rdl2/render/util/Random.h>

namespace moonshine {
namespace noise {

using namespace scene_rdl2::math;

bool Noise::sNoiseIsDataInitialized = false;
scene_rdl2::util::Random Noise::sNoiseRandom = scene_rdl2::util::Random(0xbeadceef);
std::mutex Noise::sNoiseInitDataMutex;
std::vector<int> Noise::sNoisePermutationTable;

Noise::Noise(const int seed,
             const int tableSize,
             const bool useStaticTables) :
    mRandomNumberGenerator(new scene_rdl2::util::Random(seed))
{
    mIspc.mSeed = seed % tableSize;
    mIspc.mTableSize = tableSize;

    if (useStaticTables) {
        std::scoped_lock dataLock(sNoiseInitDataMutex); // Lock this part that initializes static data
        // Only construct the static data once to share across all instances
        if (!sNoiseIsDataInitialized) {
            buildStaticTables(tableSize);
            sNoiseIsDataInitialized = true;
        }
        mIspc.mPermutationTable = sNoisePermutationTable.data();
    } else {
        buildTables(tableSize);
        mIspc.mPermutationTable = mPermutationTable.data();
    }

}

Noise::~Noise() {
    // Destroy random number generator
    delete mRandomNumberGenerator;
}

void
Noise::buildTables(const int tableSize)
{
    mPermutationTable.clear();
    mPermutationTable.reserve(tableSize);
    std::generate_n(std::back_inserter(mPermutationTable), tableSize, [i = 0] () mutable { return i++; });
    std::shuffle(mPermutationTable.begin(), mPermutationTable.end(), *mRandomNumberGenerator);
}

void
Noise::buildStaticTables(const int tableSize)
{
    sNoisePermutationTable.clear();
    sNoisePermutationTable.reserve(tableSize);
    std::generate_n(std::back_inserter(sNoisePermutationTable), tableSize, [i = 0] () mutable { return i++; });
    std::shuffle(sNoisePermutationTable.begin(), sNoisePermutationTable.end(), sNoiseRandom);
}

Xform3f 
Noise::orderedCompose(const Vec3f &translation, 
                      const Vec3f &rotation, 
                      const Vec3f &scale, 
                      const int transformationOrder, 
                      const int rotationOrder
) const {
    Xform3f rotationMat = Xform3f(scene_rdl2::math::one);
    const Vec3f xAxis = Vec3f(1.0f, 0.0f, 0.0f);
    const Vec3f yAxis = Vec3f(0.0f, 1.0f, 0.0f);
    const Vec3f zAxis = Vec3f(0.0f, 0.0f, 1.0f);

	switch (rotationOrder) {
    case ispc::NOISE_ROTATION_XYZ:
        rotationMat *= Xform3f::rotate(xAxis, rotation.x);
        rotationMat *= Xform3f::rotate(yAxis, rotation.y);
        rotationMat *= Xform3f::rotate(zAxis, rotation.z);
        break;
    case ispc::NOISE_ROTATION_XZY:
        rotationMat *= Xform3f::rotate(xAxis, rotation.x);
        rotationMat *= Xform3f::rotate(zAxis, rotation.z);
        rotationMat *= Xform3f::rotate(yAxis, rotation.y);
        break;
    case ispc::NOISE_ROTATION_YXZ:
        rotationMat *= Xform3f::rotate(yAxis, rotation.y);
        rotationMat *= Xform3f::rotate(xAxis, rotation.x);
        rotationMat *= Xform3f::rotate(zAxis, rotation.z);
        break;
    case ispc::NOISE_ROTATION_YZX:
        rotationMat *= Xform3f::rotate(yAxis, rotation.y);
        rotationMat *= Xform3f::rotate(zAxis, rotation.z);
        rotationMat *= Xform3f::rotate(xAxis, rotation.x);
        break;
    case ispc::NOISE_ROTATION_ZXY:
        rotationMat *= Xform3f::rotate(zAxis, rotation.z);
        rotationMat *= Xform3f::rotate(xAxis, rotation.x);
        rotationMat *= Xform3f::rotate(yAxis, rotation.y);
        break;
    case ispc::NOISE_ROTATION_ZYX:
        rotationMat *= Xform3f::rotate(zAxis, rotation.z);
        rotationMat *= Xform3f::rotate(yAxis, rotation.y);
        rotationMat *= Xform3f::rotate(xAxis, rotation.x);
        break;
    }

    Xform3f result = Xform3f(scene_rdl2::math::one);

    switch (transformationOrder) {
    case ispc::NOISE_XFORM_SRT:
        result = result * Xform3f::scale(scale);
        result = result * rotationMat;
        result = result * Xform3f::translate(translation);
        break;
    case ispc::NOISE_XFORM_STR:
        result = result * Xform3f::scale(scale);
        result = result * Xform3f::translate(translation);
        result = result * rotationMat;
        break;
    case ispc::NOISE_XFORM_RST:
        result = result * rotationMat;
        result = result * Xform3f::scale(scale);
        result = result * Xform3f::translate(translation);
        break;
    case ispc::NOISE_XFORM_RTS:
        result = result * rotationMat;
        result = result * Xform3f::translate(translation);
        result = result * Xform3f::scale(scale);
        break;
    case ispc::NOISE_XFORM_TSR:
        result = result * Xform3f::translate(translation);
        result = result * Xform3f::scale(scale);
        result = result * rotationMat;
        break;
    case ispc::NOISE_XFORM_TRS:
        result = result * Xform3f::translate(translation);
        result = result * rotationMat;
        result = result * Xform3f::scale(scale);
        break;
    }

    return result;
}

const ispc::NOISE_Noise*
Noise::getIspc() const
{
    return &mIspc;
}

int
Noise::perm(const int i) const
{
    return mIspc.mPermutationTable[i & (mIspc.mTableSize - 1)];
}

int
Noise::index3D(const int ix, const int iy, const int iz) const
{
    return perm(ix + perm(iy + perm(iz)));
}

int
Noise::index4D(const int ix, const int iy, const int iz, const int iw) const
{
    return perm(ix + perm(iy + perm(iz + perm(iw))));
}

} // namespace shaders
} // namespace scene_rdl2

