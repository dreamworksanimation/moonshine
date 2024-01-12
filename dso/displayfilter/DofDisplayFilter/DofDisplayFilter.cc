// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include <moonray/rendering/displayfilter/DisplayFilter.h>

#include <scene_rdl2/common/math/MathUtil.h>
#include <scene_rdl2/scene/rdl2/rdl2.h>

#include "attributes.cc"
#include "DofDisplayFilter_ispc_stubs.h"

using namespace moonray;
using namespace scene_rdl2::math;

RDL2_DSO_CLASS_BEGIN(DofDisplayFilter, DisplayFilter)

public:
    DofDisplayFilter(const SceneClass& sceneClass, const std::string& name);

    virtual void update() override;

private:
    virtual void getInputData(const moonray::displayfilter::InitializeData& initData,
                              moonray::displayfilter::InputData& inputData) const override;

    ispc::DofDisplayFilter mIspc;

RDL2_DSO_CLASS_END(DofDisplayFilter)

//---------------------------------------------------------------------------

DofDisplayFilter::DofDisplayFilter(
        const SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mFilterFuncv = (DisplayFilterFuncv) ispc::DofDisplayFilter_getFilterFunc();

    mIspc.mAperture = 0.f;
    mIspc.mFocalLength = 0.f;
    mIspc.mFocusDistance = 0.f;
    mIspc.mMask = false;
    mIspc.mInvertMask = false;
    mIspc.mMix = 0.f;
}

void
DofDisplayFilter::update()
{
    const bool noInput = get(attrInput) == nullptr;
    const bool noDepth = get(attrDepth) == nullptr;
    if (noInput) {
        fatal("Missing \"input\". attribute");
        return;
    }
    if (noDepth) {
        fatal("Missing \"depth\" attribute.");
        return;
    }
    if (noInput || noDepth) {
        return;
    }

    const SceneContext *ctx = getSceneClass().getSceneContext();
    MNRY_ASSERT_REQUIRE(ctx);
    const float sceneScale =
        ctx->getSceneVariables().get<Float>(SceneVariables::sSceneScaleKey);
    const float rsToMm = sceneScale * 1000.0f;

    if (get(attrUseCameraAttributes)) {
        const Camera* camera = ctx->getPrimaryCamera();
        MNRY_ASSERT_REQUIRE(camera);
        // This assumes the existence of these attributes
        // An exception will be thrown if they don't exist
        mIspc.mAperture = camera->get<Float>(std::string("dof_aperture")) / rsToMm;
        mIspc.mFocalLength = camera->get<Float>(std::string("focal")) / rsToMm;
        mIspc.mFocusDistance = camera->get<Float>(std::string("dof_focus_distance"));
    } else {
        mIspc.mAperture = get(attrAperture) / rsToMm;
        mIspc.mFocalLength = get(attrFocalLength) / rsToMm;
        mIspc.mFocusDistance = get(attrFocusDistance);
    }

    mIspc.mMask = get(attrMask) == nullptr ? false : true;
    mIspc.mInvertMask = get(attrInvertMask);
    mIspc.mMix = saturate(get(attrMix));
}

void
DofDisplayFilter::getInputData(const moonray::displayfilter::InitializeData& initData,
                               moonray::displayfilter::InputData& inputData) const
{
    inputData.mInputs.push_back(get(attrInput));
    inputData.mInputs.push_back(get(attrDepth));

    // A window width of 0 is actually a request for the entire frame.
    inputData.mWindowWidths.push_back(0);
    inputData.mWindowWidths.push_back(1);

    // mask
    if (get(attrMask) != nullptr) {
        inputData.mInputs.push_back(get(attrMask));
        inputData.mWindowWidths.push_back(1);
    }
}

