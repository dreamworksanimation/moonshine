// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include <moonray/rendering/displayfilter/DisplayFilter.h>

#include <scene_rdl2/common/math/Math.h>
#include <scene_rdl2/common/math/MathUtil.h>
#include <scene_rdl2/scene/rdl2/rdl2.h>

#include "attributes.cc"
#include "ToonDisplayFilter_ispc_stubs.h"

#include <cmath>

using namespace moonray;
using namespace scene_rdl2::math;

RDL2_DSO_CLASS_BEGIN(ToonDisplayFilter, DisplayFilter)

public:
    ToonDisplayFilter(const SceneClass& sceneClass, const std::string& name);

private:
    virtual void update() override;

    virtual void getInputData(const displayfilter::InitializeData& initData,
                              displayfilter::InputData& inputData) const override;

    ispc::ToonDisplayFilter mIspc;

RDL2_DSO_CLASS_END(ToonDisplayFilter)

//---------------------------------------------------------------------------

ToonDisplayFilter::ToonDisplayFilter(
        const SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mFilterFuncv = (DisplayFilterFuncv) ispc::ToonDisplayFilter_getFilterFunc();

    mIspc.mNumCels = 0;
    mIspc.mAmbient.r = 0.f;
    mIspc.mAmbient.g = 0.f;
    mIspc.mAmbient.b = 0.f;
    mIspc.mInkDepthThreshold = 0.f;
    mIspc.mInkNormalThreshold = 0.f;
    mIspc.mInkNormalScale = 0.f;
    mIspc.mEdgeDetector = 0;
}

void
ToonDisplayFilter::update()
{
    bool noInputDiffuse = get(attrInputDiffuse) == nullptr;
    bool noInputGlossy = get(attrInputGlossy) == nullptr;
    bool noInputAlbedo = get(attrInputAlbedo) == nullptr;
    bool noInputDepth = get(attrInputDepth) == nullptr;
    bool noInputNormal = get(attrInputNormal) == nullptr;
    if (noInputDiffuse) {
        fatal("Missing \"input_diffuse\" attribute.");
    }
    if (noInputGlossy) {
        fatal("Missing \"input_glossy\" attribute.");
    }
    if (noInputAlbedo) {
        fatal("Missing \"input_albedo\" attribute.");
    }
    if (noInputDepth) {
        fatal("Missing \"input_depth\" attribute.");
    }
    if (noInputNormal) {
        fatal("Missing \"input_normal\" attribute.");
    }
    if(noInputDiffuse || noInputGlossy || noInputAlbedo || noInputDepth || noInputNormal) {
        return;
    }

    mIspc.mNumCels = get(attrNumCels);
    Color ambient = get(attrAmbient);
    mIspc.mAmbient.r = ambient.r;
    mIspc.mAmbient.g = ambient.g;
    mIspc.mAmbient.b = ambient.b;
    mIspc.mInkDepthThreshold = get(attrInkDepthThreshold);
    mIspc.mInkNormalThreshold = get(attrInkNormalThreshold);
    mIspc.mInkNormalScale = get(attrInkNormalScale);
    mIspc.mEdgeDetector = get(attrEdgeDetector);
}

void
ToonDisplayFilter::getInputData(const displayfilter::InitializeData& initData,
                                displayfilter::InputData& inputData) const
{
    inputData.mInputs.push_back(get(attrInputDiffuse)); // index 0
    inputData.mInputs.push_back(get(attrInputGlossy));  // index 1
    inputData.mInputs.push_back(get(attrInputAlbedo));  // index 2
    inputData.mInputs.push_back(get(attrInputDepth));   // index 3
    inputData.mInputs.push_back(get(attrInputNormal));  // index 4

    inputData.mWindowWidths.push_back(1);
    inputData.mWindowWidths.push_back(1);
    inputData.mWindowWidths.push_back(1);
    const int edgeDetector = get(attrEdgeDetector);
    switch(edgeDetector) {
    case 1:
        inputData.mWindowWidths.push_back(3);
        inputData.mWindowWidths.push_back(3);
        break;
    case 2:
        inputData.mWindowWidths.push_back(5);
        inputData.mWindowWidths.push_back(5);
        break;
    case 3:
        inputData.mWindowWidths.push_back(9);
        inputData.mWindowWidths.push_back(9);
        break;
    default:
        inputData.mWindowWidths.push_back(1);
        inputData.mWindowWidths.push_back(1);
        break;
    }
}

//---------------------------------------------------------------------------

