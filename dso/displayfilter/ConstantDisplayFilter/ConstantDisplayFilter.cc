// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include <moonray/rendering/displayfilter/DisplayFilter.h>
#include <moonray/rendering/displayfilter/InputBuffer.h>

#include <scene_rdl2/common/math/Math.h>
#include <scene_rdl2/common/math/MathUtil.h>
#include <scene_rdl2/scene/rdl2/rdl2.h>

#include "attributes.cc"
#include "ConstantDisplayFilter_ispc_stubs.h"

using namespace moonray;

RDL2_DSO_CLASS_BEGIN(ConstantDisplayFilter, DisplayFilter)

public:
    ConstantDisplayFilter(const SceneClass& sceneClass, const std::string& name);

    virtual void update() override;

private:
    virtual void getInputData(const displayfilter::InitializeData& initData,
                              displayfilter::InputData& inputData) const override;

    ispc::ConstantDisplayFilter mIspc;

RDL2_DSO_CLASS_END(ConstantDisplayFilter)

//---------------------------------------------------------------------------

ConstantDisplayFilter::ConstantDisplayFilter(
        const SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mFilterFuncv = (DisplayFilterFuncv) ispc::ConstantDisplayFilter_getFilterFunc();

    scene_rdl2::math::asCpp(mIspc.mColor) = scene_rdl2::math::sBlack;
}

void
ConstantDisplayFilter::update()
{
    scene_rdl2::math::asCpp(mIspc.mColor) = get(attrColor);
}

void
ConstantDisplayFilter::getInputData(const displayfilter::InitializeData& initData,
                                    displayfilter::InputData& inputData) const
{}

//---------------------------------------------------------------------------

