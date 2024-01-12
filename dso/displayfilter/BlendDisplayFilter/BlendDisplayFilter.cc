// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include <moonray/rendering/displayfilter/DisplayFilter.h>
#include <moonray/rendering/displayfilter/InputBuffer.h>
#include <scene_rdl2/common/math/Math.h>
#include <scene_rdl2/common/math/MathUtil.h>
#include <scene_rdl2/scene/rdl2/rdl2.h>


#include "attributes.cc"
#include "BlendDisplayFilter_ispc_stubs.h"

using namespace moonray;

RDL2_DSO_CLASS_BEGIN(BlendDisplayFilter, DisplayFilter)

public:
    BlendDisplayFilter(const SceneClass& sceneClass, const std::string& name);

    virtual void update() override;

private:
    virtual void getInputData(const displayfilter::InitializeData& initData,
                              displayfilter::InputData& inputData) const override;

    ispc::BlendDisplayFilter mIspc;

RDL2_DSO_CLASS_END(BlendDisplayFilter)

//---------------------------------------------------------------------------

BlendDisplayFilter::BlendDisplayFilter(
        const SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mFilterFuncv = (DisplayFilterFuncv) ispc::BlendDisplayFilter_getFilterFunc();

    mIspc.mBlendAmt = 0.5f;
    mIspc.mBlendType = static_cast<ispc::BlendType>(0);
    mIspc.mMask = false;
    mIspc.mInvertMask = false;
    mIspc.mMix = 0.f;
}

void
BlendDisplayFilter::update()
{
    const bool noInput1 = get(attrInput1) == nullptr;
    const bool noInput2 = get(attrInput2) == nullptr;
    if (noInput1) {
        fatal("Missing \"input1\" attribute.");
    }
    if (noInput2) {
        fatal("Missing \"input2\" attribute.");
    }
    if (noInput1 || noInput2) {
        return;
    }

    mIspc.mBlendAmt = get(attrBlendAmt);
    mIspc.mBlendType = static_cast<ispc::BlendType>(get(attrBlendType));
    mIspc.mMask = get(attrMask) == nullptr ? false : true;
    mIspc.mInvertMask = get(attrInvertMask);
    mIspc.mMix = scene_rdl2::math::saturate(get(attrMix));
}

void
BlendDisplayFilter::getInputData(const displayfilter::InitializeData& initData,
                                 displayfilter::InputData& inputData) const
{
    inputData.mInputs.push_back(get(attrInput1));
    inputData.mInputs.push_back(get(attrInput2));

    inputData.mWindowWidths.push_back(1);
    inputData.mWindowWidths.push_back(1);

    // mask
    if (get(attrMask) != nullptr) {
        inputData.mInputs.push_back(get(attrMask));
        inputData.mWindowWidths.push_back(1);
    }
}

//---------------------------------------------------------------------------

