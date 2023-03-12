// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

static Color
sample(const uniform Map* uniform map, uniform ShadingTLState* uniform tls, const varying State& state)
{

    varying Vec3f inputNormal = evalAttrInputNormal(map, tls, state);
    float inputNormalLength = length(inputNormal);

    ReferenceFrame frame;
    ReferenceFrame_init(frame, state.mN, normalize(state.mdPds));
    const Vec3f X = frame.mX;
    const Vec3f Y = frame.mY;
    const Vec3f Z = frame.mZ;

    varying Vec3f outputNormal;
    if (getAttrTransform(map) == 0) { // tangent -> render

        // Decode the normals from [0,1] to [-1,1]
        if (getAttrDecodeInputNormal(map)) {
            inputNormal = 2.0f * inputNormal - Vec3f_ctor(1.0f, 1.0f, 1.0f);
            inputNormalLength = length(inputNormal);
            inputNormal = inputNormal / inputNormalLength; // normalize
        }

        // Transform to render space
        outputNormal = inputNormalLength * normalize(X * inputNormal.x + Y * inputNormal.y + Z * inputNormal.z);

    } else { // render -> tangent
        
        // Transform to tangent space
        // inputNormalLength is used to preserve the original length of the normal,
        // which is necessary for the Toksvig mapping technique which recalculates
        // the roughness value based on the length of the input normal in the Dwa materials.
        outputNormal = length(inputNormal) * normalize(Vec3f_ctor(dot(X, inputNormal), dot(Y, inputNormal), dot(Z, inputNormal)));

        // Encode the normals from [-1,1] to [0,1]
        outputNormal = (outputNormal * 0.5f) + Vec3f_ctor(0.5f, 0.5f, 0.5f);

    }

    return asAColor(outputNormal);
}

DEFINE_MAP_SHADER(TransformNormalMap, sample)

