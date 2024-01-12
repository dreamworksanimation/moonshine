// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include <moonray/rendering/displayfilter/DisplayFilter.h>
#include <moonray/rendering/displayfilter/InputBuffer.h>

#include <scene_rdl2/scene/rdl2/rdl2.h>
#include <scene_rdl2/common/math/Math.h>

#include "attributes.cc"
#include "ShadowDisplayFilter_ispc_stubs.h"

#include <numeric>

using namespace moonray;
using namespace scene_rdl2::math;

RDL2_DSO_CLASS_BEGIN(ShadowDisplayFilter, DisplayFilter)

public:
    ShadowDisplayFilter(const SceneClass& sceneClass, const std::string& name);
    virtual void update() override;

private:

    virtual void getInputData(const displayfilter::InitializeData& initData,
                              displayfilter::InputData& inputData) const override;

    ispc::ShadowDisplayFilter mIspc;

RDL2_DSO_CLASS_END(ShadowDisplayFilter)

//---------------------------------------------------------------------------

ShadowDisplayFilter::ShadowDisplayFilter(
        const SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mFilterFuncv = (DisplayFilterFuncv) ispc::ShadowDisplayFilter_getFilterFunc();
    mIspc.mDensity = 0.f;
    asCpp(mIspc.mShadowColor) = sBlack;
}

void
ShadowDisplayFilter::update()
{
    mIspc.mDensity = get(attrDensity);
    asCpp(mIspc.mShadowColor) = get(attrShadowColor);
}

void
ShadowDisplayFilter::getInputData(const displayfilter::InitializeData& initData,
                                  displayfilter::InputData& inputData) const
{
    inputData.mInputs.push_back(get(attrOccluded));
    inputData.mInputs.push_back(get(attrUnoccluded));
    inputData.mWindowWidths.push_back(1);
    inputData.mWindowWidths.push_back(1);
}

//---------------------------------------------------------------------------

