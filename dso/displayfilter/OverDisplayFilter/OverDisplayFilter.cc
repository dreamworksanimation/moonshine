// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include <moonray/rendering/displayfilter/DisplayFilter.h>
#include <moonray/rendering/displayfilter/InputBuffer.h>

#include <scene_rdl2/common/math/Math.h>
#include <scene_rdl2/common/math/MathUtil.h>
#include <scene_rdl2/scene/rdl2/rdl2.h>

#include "attributes.cc"
#include "OverDisplayFilter_ispc_stubs.h"

using namespace moonray;
using namespace scene_rdl2::math;

RDL2_DSO_CLASS_BEGIN(OverDisplayFilter, DisplayFilter)

public:
    OverDisplayFilter(const SceneClass& sceneClass, const std::string& name);

    virtual void update() override;

    ispc::OverDisplayFilter mIspc;

private:
    virtual void getInputData(const displayfilter::InitializeData& initData,
                              displayfilter::InputData& inputData) const override;

RDL2_DSO_CLASS_END(OverDisplayFilter)

//---------------------------------------------------------------------------

OverDisplayFilter::OverDisplayFilter(
        const SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mFilterFuncv = (DisplayFilterFuncv) ispc::OverDisplayFilter_getFilterFunc();

    mIspc.mInvertAlpha = false;
    mIspc.mMix = 0.f;
}

void
OverDisplayFilter::update()
{
    if (get(attrInputTop) == nullptr) {
        fatal("Missing \"input_top\" attribute.");
    }
    if (get(attrInputBottom) == nullptr) {
        fatal("Missing \"input_bottom\" attribute.");
    }
    if (get(attrAlpha) == nullptr) {
        fatal("Missing \"alpha\" attribute.");
    }

    mIspc.mInvertAlpha = get(attrInvertAlpha);
    mIspc.mMix = saturate(get(attrMix));
}

void
OverDisplayFilter::getInputData(const displayfilter::InitializeData& initData,
                                displayfilter::InputData& inputData) const
{
    inputData.mInputs.push_back(get(attrInputTop));
    inputData.mInputs.push_back(get(attrInputBottom));
    inputData.mInputs.push_back(get(attrAlpha));

    inputData.mWindowWidths.push_back(1);
    inputData.mWindowWidths.push_back(1);
    inputData.mWindowWidths.push_back(1);
}

//---------------------------------------------------------------------------

