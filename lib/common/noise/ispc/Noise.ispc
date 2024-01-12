// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "Noise.isph"

// Export enums and structs which will force the C++ definition 
// of ShaderNoise_Constants into NoiseBase_ispc_stubs.h - which we 
// then include in the C++ code.
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(NOISE_RotationOrders);
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(NOISE_XformOrders);
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(NOISE_Noise);

// Builds transformation matrix for noise shaders in the
// same way the Moonlight shaders did it.
varying Xform3f
NOISE_orderedCompose(const varying Vec3f &translation, 
                     const varying Vec3f &rotation, 
                     const varying Vec3f &scale, 
                     const uniform int transformationOrder, 
                     const uniform int rotationOrder)
{
    Xform3f rotationMat = Xform3f_identity();
    const Vec3f xAxis = Vec3f_ctor(1.0f, 0.0f, 0.0f);
    const Vec3f yAxis = Vec3f_ctor(0.0f, 1.0f, 0.0f);
    const Vec3f zAxis = Vec3f_ctor(0.0f, 0.0f, 1.0f);

	switch (rotationOrder) {
    case NOISE_ROTATION_XYZ:
        rotationMat = rotationMat * Xform3f_rotate(xAxis, rotation.x);
        rotationMat = rotationMat * Xform3f_rotate(yAxis, rotation.y);
        rotationMat = rotationMat * Xform3f_rotate(zAxis, rotation.z);
        break;
    case NOISE_ROTATION_XZY:
        rotationMat = rotationMat * Xform3f_rotate(xAxis, rotation.x);
        rotationMat = rotationMat * Xform3f_rotate(zAxis, rotation.z);
        rotationMat = rotationMat * Xform3f_rotate(yAxis, rotation.y);
        break;
    case NOISE_ROTATION_YXZ:
        rotationMat = rotationMat * Xform3f_rotate(yAxis, rotation.y);
        rotationMat = rotationMat * Xform3f_rotate(xAxis, rotation.x);
        rotationMat = rotationMat * Xform3f_rotate(zAxis, rotation.z);
        break;
    case NOISE_ROTATION_YZX:
        rotationMat = rotationMat * Xform3f_rotate(yAxis, rotation.y);
        rotationMat = rotationMat * Xform3f_rotate(zAxis, rotation.z);
        rotationMat = rotationMat * Xform3f_rotate(xAxis, rotation.x);
        break;
    case NOISE_ROTATION_ZXY:
        rotationMat = rotationMat * Xform3f_rotate(zAxis, rotation.z);
        rotationMat = rotationMat * Xform3f_rotate(xAxis, rotation.x);
        rotationMat = rotationMat * Xform3f_rotate(yAxis, rotation.y);
        break;
    case NOISE_ROTATION_ZYX:
        rotationMat = rotationMat * Xform3f_rotate(zAxis, rotation.z);
        rotationMat = rotationMat * Xform3f_rotate(yAxis, rotation.y);
        rotationMat = rotationMat * Xform3f_rotate(xAxis, rotation.x);
        break;
    }

    varying Xform3f result = Xform3f_identity();

    switch (transformationOrder) {
    case NOISE_XFORM_SRT:
        result = result * Xform3f_scale(scale);
        result = result * rotationMat;
        result = result * Xform3f_translate(translation);
        break;
    case NOISE_XFORM_STR:
        result = result * Xform3f_scale(scale);
        result = result * Xform3f_translate(translation);
        result = result * rotationMat;
        break;
    case NOISE_XFORM_RST:
        result = result * rotationMat;
        result = result * Xform3f_scale(scale);
        result = result * Xform3f_translate(translation);
        break;
    case NOISE_XFORM_RTS:
        result = result * rotationMat;
        result = result * Xform3f_translate(translation);
        result = result * Xform3f_scale(scale);
        break;
    case NOISE_XFORM_TSR:
        result = result * Xform3f_translate(translation);
        result = result * Xform3f_scale(scale);
        result = result * rotationMat;
        break;
    case NOISE_XFORM_TRS:
        result = result * Xform3f_translate(translation);
        result = result * rotationMat;
        result = result * Xform3f_scale(scale);
        break;
    }

    return result;
}

// Retrieves a varying random value from the permutation table
varying int
NOISE_perm(const uniform NOISE_Noise * uniform n,
           const varying int i)
{
    return n->mPermutationTable[i & (n->mTableSize - 1)];
}

// Retrieves a uniform random value from the permutation table
uniform int
NOISE_perm(const uniform NOISE_Noise * uniform n,
           const uniform int i)
{
    return n->mPermutationTable[i & (n->mTableSize - 1)];
}

// Retrieves a random value from the permutation table
// calculated recursively from the three inputs
varying int
NOISE_index3D(const uniform NOISE_Noise * uniform n,
              const varying int ix,
              const varying int iy, 
              const varying int iz)
{
    return NOISE_perm(n, ix + NOISE_perm(n, iy + NOISE_perm(n, iz)));
}

varying int
NOISE_index4D(const uniform NOISE_Noise * uniform n,
                          const varying int ix,
                          const varying int iy, 
                          const varying int iz,
                          const varying int iw)
{
    return NOISE_perm(n, ix + NOISE_perm(n, iy + NOISE_perm(n, iz + NOISE_perm(n, iw))));
}

