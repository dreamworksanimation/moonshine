// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include <moonray/rendering/displayfilter/DisplayFilter.h>
#include <moonray/rendering/displayfilter/InputBuffer.h>

#include <scene_rdl2/common/math/Math.h>
#include <scene_rdl2/common/math/MathUtil.h>
#include <scene_rdl2/scene/rdl2/rdl2.h>

#include "attributes.cc"
#include "OpDisplayFilter_ispc_stubs.h"

using namespace moonray;
using namespace scene_rdl2::math;

RDL2_DSO_CLASS_BEGIN(OpDisplayFilter, DisplayFilter)

public:
    OpDisplayFilter(const SceneClass& sceneClass, const std::string& name);

    virtual void update() override;

private:
    virtual void getInputData(const displayfilter::InitializeData& initData,
                              displayfilter::InputData& inputData) const override;

    ispc::OpDisplayFilter mIspc;

RDL2_DSO_CLASS_END(OpDisplayFilter)

//---------------------------------------------------------------------------

// If you are adding an operation that takes only one 
// input, add it here for validation purposes
bool isSingleOp(int op) {
    return op == ispc::INVERT       ||
           op == ispc::NORMALIZE    ||
           op == ispc::ABS          ||
           op == ispc::CEIL         ||
           op == ispc::FLOOR        ||
           op == ispc::LENGTH       ||
           op == ispc::SINE         ||
           op == ispc::COSINE       ||
           op == ispc::ROUND        ||
           op == ispc::ACOS         ||
           op == ispc::NOT;
}

OpDisplayFilter::OpDisplayFilter(
        const SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mFilterFuncv = (DisplayFilterFuncv) ispc::OpDisplayFilter_getFilterFunc();

    mIspc.mOperation = static_cast<ispc::OpType>(0);
    mIspc.mMask = false;
    mIspc.mInvertMask = false;
    mIspc.mMix = 0.f;
    mIspc.mIsSingleOp = false;
}

void
OpDisplayFilter::update()
{
    const bool noInput1 = get(attrInput1) == nullptr;
    const bool noInput2 = get(attrInput2) == nullptr;
    if (noInput1) {
        fatal("Missing \"input1\" attribute.");
        return;
    }
    if (!isSingleOp(get(attrOperation)) && noInput2) {
        fatal("Missing \"input2\" attribute.");
        return;
    }

    mIspc.mOperation = static_cast<ispc::OpType>(get(attrOperation));
    mIspc.mMask = get(attrMask) == nullptr ? false : true;
    mIspc.mInvertMask = get(attrInvertMask);
    mIspc.mMix = saturate(get(attrMix));
    mIspc.mIsSingleOp = noInput2;
}

void
OpDisplayFilter::getInputData(const displayfilter::InitializeData& initData,
                              displayfilter::InputData& inputData) const
{
    inputData.mInputs.push_back(get(attrInput1));
    inputData.mWindowWidths.push_back(1);

    if (get(attrInput2) != nullptr) {
        inputData.mInputs.push_back(get(attrInput2));
        inputData.mWindowWidths.push_back(1);
    }
    // mask
    if (get(attrMask) != nullptr) {
        inputData.mInputs.push_back(get(attrMask));
        inputData.mWindowWidths.push_back(1);
    }
}

//---------------------------------------------------------------------------

