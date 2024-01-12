// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include <moonray/rendering/displayfilter/DisplayFilter.h>
#include <moonray/rendering/displayfilter/InputBuffer.h>
#include <moonray/rendering/shading/RampControl.h>

#include <scene_rdl2/common/math/Math.h>
#include <scene_rdl2/common/math/MathUtil.h>
#include <scene_rdl2/scene/rdl2/rdl2.h>

#include "attributes.cc"
#include "RampDisplayFilter_ispc_stubs.h"

using namespace moonray;
using namespace scene_rdl2::math;

RDL2_DSO_CLASS_BEGIN(RampDisplayFilter, DisplayFilter)

public:
    RampDisplayFilter(const SceneClass& sceneClass, const std::string& name);

    virtual void update() override;

private:
    virtual void getInputData(const displayfilter::InitializeData& initData,
                              displayfilter::InputData& inputData) const override;
    bool validate(const std::vector<float> &positions, 
                  const std::vector<Color> &colors, 
                  const std::vector<int> &interpolations);

    ispc::RampDisplayFilter mIspc;
    moonray::shading::ColorRampControl mRampControl;

RDL2_DSO_CLASS_END(RampDisplayFilter)

/** 
 * Performs validation on user inputs & writes input values to ISPC static arrays
 * @param posPtr, @param colPtr, @param interpPtr pointer to ISPC positions/colors/interpolations arrays we are writing to
 * @param positions, @param colors, @param interpolations ref to user input positions/colors/interpolations
 */
bool RampDisplayFilter::validate(const std::vector<float> &positions, 
                                 const std::vector<Color> &colors, 
                                 const std::vector<int> &interpolations)
{
    ispc::RampInterpolator2DType rampType = static_cast<ispc::RampInterpolator2DType>(get(attrRampType));

    // ensure arrays are smallar than ISPC static array size
    if (positions.size() > ispc::MAX_POS_SIZE || colors.size() > ispc::MAX_POS_SIZE) {
        fatal("We currently do not support more than " + std::to_string(ispc::MAX_POS_SIZE) + " ramp data points");
        return false;
    }

    // ensure there is an equal number of positions, colors, and interpolations
    if (positions.size() != colors.size() || positions.size() != interpolations.size()) {
        fatal("Ramp positions, colors, and interpolations are not of the same size");
        return false;
    }

    // if it's an input ramp, it should have an input
    if (rampType == ispc::RAMP_INTERPOLATOR_2D_TYPE_INPUT && get(attrInput) == nullptr) {
        fatal("Input not provided to input ramp");
        return false;
    }

    // there should always be at least 2 control points: 0.f and 1.f
    if (positions.size() < 2) {
        fatal("You need to provide at least 2 positions");
        return false;
    }

    // sort so that smallest num appears first

    for (size_t i = 0; i < positions.size(); i++) {
        if (positions[i] < 0.f || positions[i] > 1.f) {
            fatal("Ramp: Positions must be given in the [0.0, 1.0] range");
            return false;
        }
    }

    return true;
}

//---------------------------------------------------------------------------

RampDisplayFilter::RampDisplayFilter(
        const SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mFilterFuncv = (DisplayFilterFuncv) ispc::RampDisplayFilter_getFilterFunc();
}

void
RampDisplayFilter::update()
{
    const std::vector<float>& positions = get(attrPositions);
    const std::vector<Color>& colors = get(attrColors);
    const std::vector<int>& interpolations = get(attrInterpolations);

    // validates inputs
    if (!validate(positions, colors, interpolations)) {
        return;
    }

    // sort so that smallest num appears first
    std::vector<float> positionsSorted = positions;
    std::sort(positionsSorted.begin(), positionsSorted.end());
    // ensure the first position is always 0 and the last is always 1
    positionsSorted.front() = 0.f;
    positionsSorted.back() = 1.f;

    mRampControl.init(positionsSorted.size(),
                      positionsSorted.data(),
                      colors.data(),
                      (const ispc::RampInterpolatorMode*)interpolations.data(),
                      ispc::COLOR_RAMP_CONTROL_SPACE_RGB,
                      true); // applyHueAdjustment
    mIspc.mRampControl = mRampControl.asIspc();
    mIspc.mRampType = static_cast<ispc::RampInterpolator2DType>(get(attrRampType));
    mIspc.mHasInput = get(attrInput) != nullptr;

    mIspc.mUseMask = get(attrMask) != nullptr;
    mIspc.mInvertMask = get(attrInvertMask);
    mIspc.mMix = saturate(get(attrMix));
}

void
RampDisplayFilter::getInputData(const displayfilter::InitializeData& initData,
                                displayfilter::InputData& inputData) const
{
    if (get(attrInput) != nullptr) {
        inputData.mInputs.push_back(get(attrInput));
        inputData.mWindowWidths.push_back(1);
    }

    // mask
    if (get(attrMask) != nullptr) {
        inputData.mInputs.push_back(get(attrMask));
        inputData.mWindowWidths.push_back(1);
    }
}

//---------------------------------------------------------------------------

