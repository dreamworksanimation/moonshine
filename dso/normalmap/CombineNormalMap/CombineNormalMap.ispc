// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file CombineNormalMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

struct CombineNormalMap
{
    const uniform NormalMap * uniform mNormalMap1;
    const uniform NormalMap * uniform mNormalMap2;
    uniform intptr_t mSampleNormal1Func;
    uniform intptr_t mSampleNormal2Func;
    uniform int mReversedNormalsIndx;
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(CombineNormalMap);

Vec3f
conditionNormal(const Vec3f& inputNormal,
                varying float dialValue,
                const ReferenceFrame& refFrame,
                varying float& outputLength)
{
    varying Vec3f N = Vec3f_ctor(0.0f, 0.0f, 1.0f);
    Vec3f outputNormal = inputNormal;
    dialValue = clamp(dialValue, 0.0f, 1.0f);

    // Transform render space normals to tangent space
    Vec3f tNormal = globalToLocal(refFrame,
                                  inputNormal);
    outputNormal = Vec3f_ctor(tNormal.x,
                              tNormal.y,
                              tNormal.z);

    outputLength = lerp(1.f, length(outputNormal), dialValue);
    if (isZero(outputLength)) {
        outputNormal = N;
        outputLength = 1.0f;
    } else {
        outputNormal = outputNormal / outputLength;
        if (!isOne(dialValue)) {
            outputNormal = normalize(N + (outputNormal - N) * dialValue);
        }
    }
    return outputNormal;
}

/// Combines two normal maps (usually a base map and a detail map)
/// with minimal loss of detail by Reoriented Normal Mapping.
/// See http://blog.selfshadow.com/publications/blending-in-detail/ for
/// more information and sample code
static Vec3f
sampleNormal(const uniform NormalMap* uniform  me,
             uniform ShadingTLState * uniform  tls,
             const varying State& state)
{
    const uniform CombineNormalMap * uniform cn = NORMALMAP_GET_ISPC_PTR(CombineNormalMap, me);

    varying Vec3f sample1 = Vec3f_ctor(0.0f, 0.0f, 0.0f);
    SampleNormalPtr sampleNormal1fn = (SampleNormalPtr) cn->mSampleNormal1Func;
    if (sampleNormal1fn != NULL) {
        sampleNormal1fn(cn->mNormalMap1, tls, &state, &sample1);
    }

    varying Vec3f sample2 = Vec3f_ctor(0.0f, 0.0f, 0.0f);
    SampleNormalPtr sampleNormal2fn = (SampleNormalPtr) cn->mSampleNormal2Func;
    if (sampleNormal2fn != NULL) {
        sampleNormal2fn(cn->mNormalMap2, tls, &state, &sample2);
    }

    float l1, l2;
    varying Vec3f n1, n2;
    varying bool reversedNormals = false;
    if (isProvided(state, cn->mReversedNormalsIndx)) {
        reversedNormals = getBoolAttribute(tls, state, cn->mReversedNormalsIndx);
    }
    const Vec3f statedPds = reversedNormals ? state.mdPds * -1.0f : state.mdPds * 1.0f;
    const ReferenceFrame refFrame = ReferenceFrame_ctor(getN(state), normalize(statedPds));

    n1 = conditionNormal(sample1,
                         evalAttrNormalMap1Dial(me, tls, state),
                         refFrame,
                         l1);

    n2 = conditionNormal(sample2,
                         evalAttrNormalMap2Dial(me, tls, state),
                         refFrame,
                         l2);

    // Reoriented Normal Mapping
    const varying Vec3f t = Vec3f_ctor(n1.x, n1.y, n1.z + 1.0f);
    const varying Vec3f u = Vec3f_ctor(-1.0f * n2.x, -1.0f * n2.y, n2.z);

    // l1 and l2 are used to preserve the original length of the normal,
    // which is necessary for the Toksvig mapping technique which recalculates
    // the roughness value based on the length of the input normal in the Dwa materials.
    Vec3f result = (l1 + l2) * 0.5f * normalize(t * dot(t, u) - u * t.z);

    // Make sure the combined normal is not pointing below the hemisphere of unbent normal
    if (result.z <= 0.0f) {
        result.z = sEpsilon;
        result = normalize(result);
    }

    return localToGlobal(refFrame, result);
}

DEFINE_NORMALMAP_SHADER(CombineNormalMap, sampleNormal)

