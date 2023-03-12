// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include <moonray/rendering/displayfilter/DisplayFilter.h>
#include <moonray/rendering/displayfilter/InputBuffer.h>

#include <scene_rdl2/common/math/Math.h>
#include <scene_rdl2/common/math/MathUtil.h>
#include <scene_rdl2/scene/rdl2/rdl2.h>

#include "attributes.cc"
#include "ConvolutionDisplayFilter_ispc_stubs.h"

#include <numeric>

using namespace moonray;

RDL2_DSO_CLASS_BEGIN(ConvolutionDisplayFilter, DisplayFilter)

public:
    ConvolutionDisplayFilter(const SceneClass& sceneClass, const std::string& name);

    virtual void update() override;

private:
    virtual void getInputData(const displayfilter::InitializeData& initData,
                              displayfilter::InputData& inputData) const override;

    ispc::ConvolutionDisplayFilter mIspc;

    std::vector<float> mKernel;
    size_t mKernelSize;

RDL2_DSO_CLASS_END(ConvolutionDisplayFilter)

//---------------------------------------------------------------------------

enum KernelType {
    GAUSSIAN = 0,
    BOX = 1,
    CUSTOM = 2,
};

ConvolutionDisplayFilter::ConvolutionDisplayFilter(
        const SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mFilterFuncv = (DisplayFilterFuncv) ispc::ConvolutionDisplayFilter_getFilterFunc();

    mIspc.mKernel = nullptr;
    mIspc.mKernelSize = 0;
    mIspc.mMask = false;
    mIspc.mInvertMask = false;
    mIspc.mMix = 0.f;
}

void
ConvolutionDisplayFilter::update()
{
    if (get(attrInput) == nullptr) {
        fatal("Missing \"input\" attribute.");
        return;
    }

    // get kernel size, make sure it is an odd number
    size_t kernelSize = get(attrKernelSize);
    if (kernelSize % 2 == 0) {
        kernelSize++;
    }
    mKernelSize = kernelSize;

    KernelType kernelType = static_cast<KernelType>(get(attrKernelType));
    size_t kernelSizeSqr = kernelSize * kernelSize;
    switch (kernelType) {
    case GAUSSIAN:
    {
        mKernel.resize(kernelSizeSqr);
        // could be a user parameter
        float sigma = 1.f / float(kernelSize);
        float sum = 0;
        size_t center = kernelSize / 2;
        for (size_t y = 0; y < kernelSize; ++y) {
            for (size_t x = 0; x < kernelSize; ++x) {
                mKernel[x + y * kernelSize] =
                    sigma * scene_rdl2::math::exp(-sigma * ((x-center) * (x - center) + (y-center) * (y - center)));

                // Accumulate the kernel values
                sum += mKernel[x + y * kernelSize];
            }
        }
        for (float& val : mKernel) {
            val /= sum;
        }
        break;
    }
    case BOX:
    {
        float val = 1.f / kernelSizeSqr;
        mKernel.resize(kernelSizeSqr);
        std::fill(mKernel.begin(), mKernel.end(), val);
        break;
    }
    case CUSTOM:
    {
        // assume the kernel is well formed: sqaure of odd number size
        mKernel = get(attrCustomKernel);
        float sum = std::accumulate(mKernel.begin(), mKernel.end(), 0.f);
        if (!scene_rdl2::math::isZero(sum) && !scene_rdl2::math::isOne(sum)) {
            for (float& val : mKernel) {
                val /= sum;
            }
        }
        // reset kernel size
        mKernelSize = (size_t)scene_rdl2::math::sqrt((float)mKernel.size());
        break;
    }
    default:
        //identity
        mKernel.resize(kernelSizeSqr);
        std::fill(mKernel.begin(), mKernel.end(), 0.f);
        mKernel[kernelSizeSqr / 2] = 1.f;
        break;
    }

    mIspc.mKernelSize = mKernelSize;
    mIspc.mKernel = mKernel.data();

    mIspc.mMask = get(attrMask) == nullptr ? false : true;
    mIspc.mInvertMask = get(attrInvertMask);
    mIspc.mMix = scene_rdl2::math::saturate(get(attrMix));
}

void
ConvolutionDisplayFilter::getInputData(const displayfilter::InitializeData& initData,
                                       displayfilter::InputData& inputData) const
{
    inputData.mInputs.push_back(get(attrInput));
    inputData.mWindowWidths.push_back(mKernelSize);

    // mask
    if (get(attrMask) != nullptr) {
        inputData.mInputs.push_back(get(attrMask));
        inputData.mWindowWidths.push_back(1);
    }
}

//---------------------------------------------------------------------------

