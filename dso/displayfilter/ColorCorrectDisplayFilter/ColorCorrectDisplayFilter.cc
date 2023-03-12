// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include <moonray/rendering/displayfilter/DisplayFilter.h>
#include <moonray/rendering/displayfilter/InputBuffer.h>

#include <scene_rdl2/common/math/Math.h>
#include <scene_rdl2/common/math/MathUtil.h>
#include <scene_rdl2/scene/rdl2/rdl2.h>

#include "attributes.cc"
#include "ColorCorrectDisplayFilter_ispc_stubs.h"

using namespace moonray;

RDL2_DSO_CLASS_BEGIN(ColorCorrectDisplayFilter, DisplayFilter)

public:
    ColorCorrectDisplayFilter(const SceneClass& sceneClass, const std::string& name);

    virtual void update() override;

private:
    virtual void getInputData(const displayfilter::InitializeData& initData,
                              displayfilter::InputData& inputData) const override;

    ispc::ColorCorrectDisplayFilter mIspc;

RDL2_DSO_CLASS_END(ColorCorrectDisplayFilter)

//---------------------------------------------------------------------------

ColorCorrectDisplayFilter::ColorCorrectDisplayFilter(
        const SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mFilterFuncv = (DisplayFilterFuncv) ispc::ColorCorrectDisplayFilter_getFilterFunc();

    mIspc.mExposure = 0.f;
    mIspc.mSaturation = 0.f;
    mIspc.mContrast = 0.f;
    mIspc.mGamma = 0.f;
    mIspc.mOffset = reinterpret_cast<const ispc::Col3f &>(scene_rdl2::math::sBlack);
    mIspc.mMultiply = reinterpret_cast<const ispc::Col3f &>(scene_rdl2::math::sBlack);
    mIspc.mMask = false;
    mIspc.mInvertMask = false;
    mIspc.mMix = 0.f;
}

void
ColorCorrectDisplayFilter::update()
{
    if (get(attrInput) == nullptr) {
        fatal("Missing \"input\" attribute");
        return;
    }
    mIspc.mExposure = get(attrExposure);
    mIspc.mSaturation = get(attrSaturation);
    mIspc.mContrast = get(attrContrast);
    mIspc.mGamma = get(attrGamma);
    mIspc.mOffset = reinterpret_cast<const ispc::Col3f &>(get(attrOffset));
    mIspc.mMultiply = reinterpret_cast<const ispc::Col3f &>(get(attrMultiply));
    mIspc.mMask = get(attrMask) == nullptr ? false : true;
    mIspc.mInvertMask = get(attrInvertMask);
    mIspc.mMix = scene_rdl2::math::saturate(get(attrMix));
}

void
ColorCorrectDisplayFilter::getInputData(const displayfilter::InitializeData& initData,
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

