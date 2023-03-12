// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file CombineNormalMap.cc

#include "attributes.cc"
#include "CombineNormalMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>

using namespace scene_rdl2::math;

RDL2_DSO_CLASS_BEGIN(CombineNormalMap, NormalMap)
public:
    CombineNormalMap(SceneClass const &sceneClass,
                     std::string const &name);
    void update();

private:
    static void sampleNormal(const NormalMap *self,
                             moonray::shading::TLState *tls,
                             const moonray::shading::State &state,
                             Vec3f *sample);

    bool verifyInputs();

    ispc::CombineNormalMap mIspc;
    NormalMap* mNormalMap1;
    NormalMap* mNormalMap2;

RDL2_DSO_CLASS_END(CombineNormalMap)

CombineNormalMap::CombineNormalMap(const SceneClass& sceneClass,
                                   const std::string& name) :
    Parent(sceneClass, name)
{
    mSampleNormalFunc = CombineNormalMap::sampleNormal;
    mSampleNormalFuncv = (SampleNormalFuncv)ispc::CombineNormalMap_getSampleFunc();
}

bool
CombineNormalMap::verifyInputs() {
    if (mNormalMap1 == nullptr && mNormalMap2 == nullptr) {
        error("CombineNormalMap: No input normal objects provided.");
        return false;
    } else if (mNormalMap1 == nullptr) {
        error("CombineNormalMap: Input 1 normal object not provided.");
        return false;
    } else if (mNormalMap2 == nullptr) {
        error("CombineNormalMap: Input 2 normal object not provided.");
        return false;
    }

    return true;
}

void
CombineNormalMap::update()
{
    mNormalMap1 = get(attrInput1) ?
                  get(attrInput1)->asA<NormalMap>() :
                  nullptr;

    mNormalMap2 = get(attrInput2) ?
                  get(attrInput2)->asA<NormalMap>() :
                  nullptr;

    if (!verifyInputs()) {
        fatal("CombineNormalMap nor connected properly");
        return;
    }

    mIspc.mNormalMap1 = mNormalMap1;
    mIspc.mSampleNormal1Func = (mNormalMap1 != nullptr) ?
                               (intptr_t) mNormalMap1->mSampleNormalFuncv :
                               (intptr_t) nullptr;

    mIspc.mNormalMap2 = mNormalMap2;
    mIspc.mSampleNormal2Func = (mNormalMap2 != nullptr) ?
                               (intptr_t) mNormalMap2->mSampleNormalFuncv :
                               (intptr_t) nullptr;

    // Get whether or not the normals have been reversed
    mOptionalAttributes.push_back(moonray::shading::StandardAttributes::sReversedNormals);
    mIspc.mReversedNormalsIndx = moonray::shading::StandardAttributes::sReversedNormals.getIndex();
}

Vec3f
conditionNormal(const Vec3f& inputNormal,
                float dialValue,
                const ReferenceFrame& refFrame,
                float& outputLength)
{
    Vec3f N(0.0f, 0.0f, 1.0f);
    Vec3f outputNormal = inputNormal;
    dialValue = clamp(dialValue, 0.0f, 1.0f);

    // Transform render space normals to tangent space
    outputNormal = refFrame.globalToLocal(inputNormal);

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
void
CombineNormalMap::sampleNormal(const NormalMap* self,
                               moonray::shading::TLState *tls,
                               const moonray::shading::State& state,
                               Vec3f* sample)
{
    const CombineNormalMap* me = static_cast<const CombineNormalMap*>(self);


    MNRY_ASSERT(me->mNormalMap1 != nullptr);
    Vec3f sample1(0.0f);
    me->mNormalMap1->sampleNormal(tls, state, &sample1);

    MNRY_ASSERT(me->mNormalMap2 != nullptr);
    Vec3f sample2(0.0f);
    me->mNormalMap2->sampleNormal(tls, state, &sample2);

    float l1, l2;
    Vec3f n1, n2;
    bool reversedNormals = false;
    if (state.isProvided(moonray::shading::StandardAttributes::sReversedNormals)) {
        reversedNormals = state.getAttribute(moonray::shading::StandardAttributes::sReversedNormals);
    }
    const scene_rdl2::math::Vec3f statedPds = reversedNormals ? state.getdPds() * -1.0f : state.getdPds() * 1.0f;
    const ReferenceFrame refFrame(state.getN(), scene_rdl2::math::normalize(statedPds));

    n1 = conditionNormal(sample1,
                         evalFloat(me, attrNormalMap1Dial, tls, state),
                         refFrame,
                         l1);

    n2 = conditionNormal(sample2,
                         evalFloat(me, attrNormalMap2Dial, tls, state),
                         refFrame,
                         l2);

    // Reoriented Normal Mapping
    const Vec3f t = Vec3f(n1.x, n1.y, n1.z + 1.0f);
    const Vec3f u = Vec3f(-n2.x, -n2.y, n2.z);

    // l1 and l2 are used to preserve the original length of the normal,
    // which is necessary for the Toksvig mapping technique which recalculates
    // the roughness value based on the length of the input normal in the Dwa materials.
    Vec3f result = (l1 + l2) * 0.5f * normalize(t * dot(t, u) - u * t.z);

    // Make sure the combined normal is not pointing below the hemisphere of unbent normal
    if (result.z <= 0.0f) {
        result.z = sEpsilon;
        result = normalize(result);
    }

    // Transform tangent space normals to render space
    result = refFrame.localToGlobal(result);

    *sample = result;
}

