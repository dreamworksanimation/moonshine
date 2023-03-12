// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include <moonray/rendering/displayfilter/DisplayFilter.h>
#include <scene_rdl2/common/math/Color.h>
#include <scene_rdl2/common/math/Math.h>
#include <scene_rdl2/common/math/MathUtil.h>
#include <scene_rdl2/scene/rdl2/rdl2.h>

#include "attributes.cc"
#include "RemapDisplayFilter_ispc_stubs.h"

using namespace moonray;
using namespace scene_rdl2::math;

RDL2_DSO_CLASS_BEGIN(RemapDisplayFilter, DisplayFilter)

public:
    RemapDisplayFilter(const SceneClass& sceneClass, const std::string& name);

    virtual void update() override;

private:
    virtual void getInputData(const displayfilter::InitializeData& initData,
                              displayfilter::InputData& inputData) const override;

    ispc::RemapDisplayFilter mIspc;

RDL2_DSO_CLASS_END(RemapDisplayFilter)

//---------------------------------------------------------------------------

RemapDisplayFilter::RemapDisplayFilter(
        const SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mFilterFuncv = (DisplayFilterFuncv) ispc::RemapDisplayFilter_getFilterFunc();
    mIspc.mUseMask = false;
    mIspc.mInvertMask = false;
    mIspc.mMix = 0.f;
}

void
RemapDisplayFilter::update()
{
    if (get(attrInput) == nullptr) {
        fatal("Missing \"input\" attribute");
        return;
    }

    mIspc.mInMin                  = get(attrInMin);
    mIspc.mInMax                  = get(attrInMax);
    mIspc.mOutMin                 = get(attrOutMin);
    mIspc.mOutMax                 = get(attrOutMax);
    mIspc.mBiasAmount             = get(attrBias);

    asCpp(mIspc.mInMinRGB)        = get(attrInMinRGB);
    asCpp(mIspc.mInMaxRGB)        = get(attrInMaxRGB);
    asCpp(mIspc.mOutMinRGB)       = get(attrOutMinRGB);
    asCpp(mIspc.mOutMaxRGB)       = get(attrOutMaxRGB);
    asCpp(mIspc.mBiasAmountRGB)   = get(attrBiasRGB);
    mIspc.mRemapMethod            = static_cast<ispc::RemapMethod>(get(attrRemapMethod));

    mIspc.mClamp                  = get(attrClamp);
    mIspc.mClampMin               = get(attrClampMin);
    mIspc.mClampMax               = get(attrClampMax);
    mIspc.mClampRGB               = get(attrClampRGB);
    asCpp(mIspc.mClampRGBMin)     = get(attrClampMinRGB);
    asCpp(mIspc.mClampRGBMax)     = get(attrClampMaxRGB);

    mIspc.mUseMask                = get(attrMask) != nullptr;
    mIspc.mInvertMask             = get(attrInvertMask);
    mIspc.mMix                    = saturate(get(attrMix));
}

void
RemapDisplayFilter::getInputData(const displayfilter::InitializeData& initData,
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

