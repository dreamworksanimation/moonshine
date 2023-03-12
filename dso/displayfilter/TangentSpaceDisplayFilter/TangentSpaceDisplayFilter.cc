// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include <moonray/rendering/displayfilter/DisplayFilter.h>
#include <moonray/rendering/displayfilter/InputBuffer.h>

#include <scene_rdl2/common/math/Math.h>
#include <scene_rdl2/common/math/MathUtil.h>
#include <scene_rdl2/scene/rdl2/rdl2.h>

#include "attributes.cc"
#include "TangentSpaceDisplayFilter_ispc_stubs.h"

RDL2_DSO_CLASS_BEGIN(TangentSpaceDisplayFilter, DisplayFilter)

public:
    TangentSpaceDisplayFilter(const SceneClass& sceneClass, const std::string& name);

    virtual void update() override;

private:
    virtual void getInputData(const moonray::displayfilter::InitializeData& initData,
                              moonray::displayfilter::InputData& inputData) const override;

    ispc::TangentSpaceDisplayFilter mIspc;

RDL2_DSO_CLASS_END(TangentSpaceDisplayFilter)

//---------------------------------------------------------------------------

TangentSpaceDisplayFilter::TangentSpaceDisplayFilter(
    const SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mFilterFuncv = (DisplayFilterFuncv) ispc::TangentSpaceDisplayFilter_getFilterFunc();

    mIspc.mMask = false;
    mIspc.mInvertMask = false;
    mIspc.mNormalMapOutput = true;
}

void
TangentSpaceDisplayFilter::update()
{
    const bool noInput = get(attrInput) == nullptr;
    const bool noNormal = get(attrNormal) == nullptr;
    const bool nodPds = get(attrdPds) == nullptr;
    if (noInput) {
        fatal("Missing \"input\" attribute.");
    }
    if (noNormal) {
        fatal("Missing \"normal\" attribute.");
    }
    if (nodPds) {
        fatal("Missing \"dPdS\" attribute.");
    }
    if(noInput || noNormal || nodPds) {
        return;
    }

    mIspc.mNormalMapOutput = get(attrNormalMapOutput);
    mIspc.mMask = get(attrMask) == nullptr ? false : true;
    mIspc.mInvertMask = get(attrInvertMask);
}

void
TangentSpaceDisplayFilter::getInputData(const moonray::displayfilter::InitializeData& initData,
                                        moonray::displayfilter::InputData& inputData) const
{
    inputData.mInputs.push_back(get(attrInput));
    inputData.mInputs.push_back(get(attrNormal));
    inputData.mInputs.push_back(get(attrdPds));

    inputData.mWindowWidths.push_back(1);
    inputData.mWindowWidths.push_back(1);
    inputData.mWindowWidths.push_back(1);

    // mask
    if (get(attrMask) != nullptr) {
        inputData.mInputs.push_back(get(attrMask));
        inputData.mWindowWidths.push_back(1);
    }
}

//---------------------------------------------------------------------------

