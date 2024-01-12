// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include <moonray/rendering/displayfilter/DisplayFilter.h>
#include <moonray/rendering/displayfilter/InputBuffer.h>

#include <scene_rdl2/common/math/Math.h>
#include <scene_rdl2/common/math/MathUtil.h>
#include <scene_rdl2/scene/rdl2/rdl2.h>

#include "attributes.cc"
#include "DiscretizeDisplayFilter_ispc_stubs.h"

using namespace moonray;
using namespace scene_rdl2::math;

RDL2_DSO_CLASS_BEGIN(DiscretizeDisplayFilter, DisplayFilter)

public:
    DiscretizeDisplayFilter(const SceneClass& sceneClass, const std::string& name);

    virtual void update() override;

private:
    virtual void getInputData(const displayfilter::InitializeData& initData,
                              displayfilter::InputData& inputData) const override;

    ispc::DiscretizeDisplayFilter mIspc;

RDL2_DSO_CLASS_END(DiscretizeDisplayFilter)

//---------------------------------------------------------------------------

DiscretizeDisplayFilter::DiscretizeDisplayFilter(
        const SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mFilterFuncv = (DisplayFilterFuncv) ispc::DiscretizeDisplayFilter_getFilterFunc();

    mIspc.mNumBins = 0.f;
    mIspc.mMask = false;
    mIspc.mInvertMask = false;
    mIspc.mMix = 0.f;
}

void
DiscretizeDisplayFilter::update()
{
    if (get(attrInput) == nullptr) {
        fatal("Missing \"input\" attribute");
        return;
    }
    mIspc.mNumBins = scene_rdl2::math::max(1, get(attrNumBins));
    mIspc.mMask = get(attrMask) == nullptr ? false : true;
    mIspc.mInvertMask = get(attrInvertMask);
    mIspc.mMix = saturate(get(attrMix));
}

void
DiscretizeDisplayFilter::getInputData(const displayfilter::InitializeData& initData,
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

