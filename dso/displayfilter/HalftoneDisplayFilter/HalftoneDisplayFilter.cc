// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include <moonray/rendering/displayfilter/DisplayFilter.h>

#include <scene_rdl2/common/math/Math.h>
#include <scene_rdl2/common/math/MathUtil.h>
#include <scene_rdl2/scene/rdl2/rdl2.h>

#include "attributes.cc"
#include "HalftoneDisplayFilter_ispc_stubs.h"

using namespace moonray;
using namespace scene_rdl2::math;

RDL2_DSO_CLASS_BEGIN(HalftoneDisplayFilter, DisplayFilter)

public:
    HalftoneDisplayFilter(const SceneClass& sceneClass, const std::string& name);

    virtual void update() override;

private:
    virtual void getInputData(const displayfilter::InitializeData& initData,
                              displayfilter::InputData& inputData) const override;

    ispc::HalftoneDisplayFilter mIspc;

RDL2_DSO_CLASS_END(HalftoneDisplayFilter)

//---------------------------------------------------------------------------

HalftoneDisplayFilter::HalftoneDisplayFilter(
        const SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mFilterFuncv = (DisplayFilterFuncv) ispc::HalftoneDisplayFilter_getFilterFunc();

    mIspc.mSize = 0;
    mIspc.mFilterWidth = 0.f;
    mIspc.mInvert = false;
    mIspc.mGrayscale = false;
    mIspc.mMask = false;
    mIspc.mInvertMask = false;
    mIspc.mMix = 0.f;
}

void
HalftoneDisplayFilter::update()
{
    if (get(attrInput) == nullptr) {
        fatal("Missing \"input\" attribute.");
        return;
    }

    mIspc.mSize = max(get(attrSize), 3);
    mIspc.mFilterWidth = scene_rdl2::math::max(0.25f, get(attrFilterWidth) * 0.5f) / (float)mIspc.mSize;
    mIspc.mInvert = get(attrInvert);
    mIspc.mGrayscale = get(attrGrayscale);
    mIspc.mMask = get(attrMask) == nullptr ? false : true;
    mIspc.mInvertMask = get(attrInvertMask);
    mIspc.mMix = saturate(get(attrMix));
}

void
HalftoneDisplayFilter::getInputData(const displayfilter::InitializeData& initData,
                                    displayfilter::InputData& inputData) const
{
    inputData.mInputs.push_back(get(attrInput));
    inputData.mWindowWidths.push_back(2 * get(attrSize) + 1);

    // mask
    if (get(attrMask) != nullptr) {
        inputData.mInputs.push_back(get(attrMask));
        inputData.mWindowWidths.push_back(1);
    }
}

//---------------------------------------------------------------------------

