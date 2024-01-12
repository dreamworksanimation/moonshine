// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include <moonray/rendering/displayfilter/DisplayFilter.h>
#include <moonray/rendering/displayfilter/InputBuffer.h>

#include <scene_rdl2/common/math/Math.h>
#include <scene_rdl2/common/math/MathUtil.h>
#include <scene_rdl2/scene/rdl2/rdl2.h>

#include "attributes.cc"
#include "ClampDisplayFilter_ispc_stubs.h"

using namespace moonray;

RDL2_DSO_CLASS_BEGIN(ClampDisplayFilter, DisplayFilter)

public:
    ClampDisplayFilter(const SceneClass& sceneClass, const std::string& name);

    virtual void update() override;

private:
    virtual void getInputData(const displayfilter::InitializeData& initData,
                              displayfilter::InputData& inputData) const override;

    ispc::ClampDisplayFilter mIspc;

RDL2_DSO_CLASS_END(ClampDisplayFilter)

//---------------------------------------------------------------------------

ClampDisplayFilter::ClampDisplayFilter(
        const SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mFilterFuncv = (DisplayFilterFuncv) ispc::ClampDisplayFilter_getFilterFunc();

    mIspc.mMin = reinterpret_cast<const ispc::Col3f &>(scene_rdl2::math::sBlack);
    mIspc.mMax = reinterpret_cast<const ispc::Col3f &>(scene_rdl2::math::sBlack);
    mIspc.mMask = false;
    mIspc.mInvertMask = false;
    mIspc.mMix = 0.f;
}

void
ClampDisplayFilter::update()
{
    if (get(attrInput) == nullptr) {
        fatal("Missing \"input\" attribute");
        return;
    }
    mIspc.mMin = reinterpret_cast<const ispc::Col3f &>(get(attrMin));
    mIspc.mMax = reinterpret_cast<const ispc::Col3f &>(get(attrMax));
    mIspc.mMask = get(attrMask) == nullptr ? false : true;
    mIspc.mInvertMask = get(attrInvertMask);
    mIspc.mMix = scene_rdl2::math::saturate(get(attrMix));
}

void
ClampDisplayFilter::getInputData(const displayfilter::InitializeData& initData,
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

