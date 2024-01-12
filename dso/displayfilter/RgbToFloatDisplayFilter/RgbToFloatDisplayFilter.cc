// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include <moonray/rendering/displayfilter/DisplayFilter.h>
#include <moonray/rendering/displayfilter/InputBuffer.h>

#include <scene_rdl2/common/math/Math.h>
#include <scene_rdl2/common/math/MathUtil.h>
#include <scene_rdl2/scene/rdl2/rdl2.h>

#include "attributes.cc"
#include "RgbToFloatDisplayFilter_ispc_stubs.h"

using namespace moonray;
using namespace scene_rdl2::math;

RDL2_DSO_CLASS_BEGIN(RgbToFloatDisplayFilter, DisplayFilter)

public:
    RgbToFloatDisplayFilter(const SceneClass& sceneClass, const std::string& name);

    virtual void update() override;

private:
    virtual void getInputData(const displayfilter::InitializeData& initData,
                              displayfilter::InputData& inputData) const override;

    ispc::RgbToFloatDisplayFilter mIspc;

RDL2_DSO_CLASS_END(RgbToFloatDisplayFilter)

//---------------------------------------------------------------------------

RgbToFloatDisplayFilter::RgbToFloatDisplayFilter(
        const SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mFilterFuncv = (DisplayFilterFuncv) ispc::RgbToFloatDisplayFilter_getFilterFunc();

    mIspc.mMode = static_cast<ispc::RgbToFloatMode>(0);
    mIspc.mMask = false;
    mIspc.mInvertMask = false;
    mIspc.mMix = 0.f;
}

void
RgbToFloatDisplayFilter::update()
{
    if(get(attrInput) == nullptr) {
        fatal("Missing \"input\" attribute.");
        return;
    }

    mIspc.mMode = static_cast<ispc::RgbToFloatMode>(get(attrMode));
    mIspc.mMask = get(attrMask) == nullptr ? false : true;
    mIspc.mInvertMask = get(attrInvertMask);
    mIspc.mMix = saturate(get(attrMix));
}

void
RgbToFloatDisplayFilter::getInputData(const displayfilter::InitializeData& initData,
                                      displayfilter::InputData& inputData) const
{
    inputData.mInputs.push_back(get(attrInput));
    inputData.mWindowWidths.push_back(1);

    // mask
    if (get(attrMask) != nullptr) {
        inputData.mInputs.push_back(get(attrMask));
        inputData.mWindowWidths.push_back(1);
    }
}

//---------------------------------------------------------------------------

