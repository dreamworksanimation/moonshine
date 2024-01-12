// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.cc"
#include "TransformNormalMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>

using namespace scene_rdl2::math;

//----------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(TransformNormalMap, Map)

public:
    TransformNormalMap(const SceneClass& sceneClass, const std::string& name);
    ~TransformNormalMap() override;
    void update() override;

private:
    static void sample(const Map* self, moonray::shading::TLState* tls, const moonray::shading::State& state, Color* sample);

RDL2_DSO_CLASS_END(TransformNormalMap)

//----------------------------------------------------------------------------

TransformNormalMap::TransformNormalMap(const SceneClass& sceneClass, const std::string& name)
    : Parent(sceneClass, name)
{
    mSampleFunc = TransformNormalMap::sample;
    mSampleFuncv = (SampleFuncv) ispc::TransformNormalMap_getSampleFunc();
}

TransformNormalMap::~TransformNormalMap()
{
}

void
TransformNormalMap::update()
{
}

void
TransformNormalMap::sample(const Map* self, moonray::shading::TLState* tls, const moonray::shading::State& state, Color* sample)
{
    const TransformNormalMap* me = static_cast<const TransformNormalMap*>(self);

    Vec3f inputNormal = evalVec3f(me, attrInputNormal, tls, state);
    float inputNormalLength = length(inputNormal);

    const ReferenceFrame frame(state.getN(), normalize(state.getdPds()));

    Vec3f outputNormal;
    if (me->get(attrTransform) == 0) { // tangent -> render

        // Decode the normals from [0,1] to [-1,1]
        if (me->get(attrDecodeInputNormal)) {
            inputNormal = 2.0f * inputNormal - Vec3f(1.0f);
            inputNormalLength = length(inputNormal);
            inputNormal = inputNormal / inputNormalLength; // normalize
        }

        // Transform to render space
        outputNormal = inputNormalLength * normalize(frame.localToGlobal(inputNormal));

    } else { // render -> tangent

        // Transform to tangent space
        // inputNormalLength is used to preserve the original length of the normal,
        // which is necessary for the Toksvig mapping technique which recalculates
        // the roughness value based on the length of the input normal in the Dwa materials.
        outputNormal = length(inputNormal) * normalize(frame.globalToLocal(inputNormal));

        // Encode the normals from [-1,1] to [0,1]
        outputNormal = (outputNormal * 0.5f) + Vec3f(0.5f);

    }

    *sample = Color(outputNormal.x, outputNormal.y, outputNormal.z);
}

