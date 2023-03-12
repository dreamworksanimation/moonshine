// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include <moonray/rendering/displayfilter/DisplayFilter.h>
#include <moonray/rendering/displayfilter/InputBuffer.h>

#include "attributes.cc"
#include "RgbToHsvDisplayFilter_ispc_stubs.h"

using namespace moonray;

RDL2_DSO_CLASS_BEGIN(RgbToHsvDisplayFilter, DisplayFilter)

public:
    RgbToHsvDisplayFilter(const SceneClass& sceneClass, const std::string& name);

    virtual void update() override;

private:
    virtual void getInputData(const displayfilter::InitializeData& initData,
                              displayfilter::InputData& inputData) const override;

    ispc::RgbToHsvDisplayFilter mIspc;

RDL2_DSO_CLASS_END(RgbToHsvDisplayFilter)

//---------------------------------------------------------------------------

RgbToHsvDisplayFilter::RgbToHsvDisplayFilter(
        const SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mFilterFuncv = (DisplayFilterFuncv) ispc::RgbToHsvDisplayFilter_getFilterFunc();

    mIspc.mMode = static_cast<ispc::RgbToHsvMode>(0);
}

void
RgbToHsvDisplayFilter::update()
{
    if(get(attrInput) == nullptr) {
        fatal("Missing \"input\" attribute.");
        return;
    }
    mIspc.mMode = static_cast<ispc::RgbToHsvMode>(get(attrMode));
}

void
RgbToHsvDisplayFilter::getInputData(const displayfilter::InitializeData& initData,
                                      displayfilter::InputData& inputData) const
{
    inputData.mInputs.push_back(get(attrInput));
    inputData.mWindowWidths.push_back(1);
}

//---------------------------------------------------------------------------

