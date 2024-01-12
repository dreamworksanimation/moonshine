// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include <moonray/rendering/displayfilter/DisplayFilter.isph>
#include <moonray/rendering/shading/ispc/Shading.isph>
#include <scene_rdl2/common/math/ispc/Math.isph>
#include <scene_rdl2/common/math/ispc/asA.isph>

struct TangentSpaceDisplayFilter
{
    bool mMask;
    bool mInvertMask;
    bool mNormalMapOutput;
};

export const uniform TangentSpaceDisplayFilter * uniform
TangentSpaceDisplayFilter_get(const uniform DisplayFilter * uniform displayFilter)
{
    return DISPLAYFILTER_GET_ISPC_CPTR(TangentSpaceDisplayFilter, displayFilter);
}

static void
filter(const uniform DisplayFilter * uniform me,
       const uniform InputBuffer * const uniform * const uniform inputBuffers,
       const varying DisplayFilterState * const uniform state,
       varying Color * uniform result)
{
    const uniform TangentSpaceDisplayFilter * uniform self = TangentSpaceDisplayFilter_get(me);

    // input buffers given to us are in the same order as the
    // inputs SceneObjects we returned from getInputData();
    const uniform InputBuffer const * uniform inBuffer = inputBuffers[0];
    const varying Color inputColor = InputBuffer_getPixel(inBuffer,
                                                          state->mOutputPixelX,
                                                          state->mOutputPixelY);
    varying Vec3f input = Vec3f_ctor(inputColor.r, inputColor.g, inputColor.b);

    const uniform InputBuffer const * uniform normalBuffer = inputBuffers[1];
    const varying Color inputNormal = InputBuffer_getPixel(normalBuffer,
                                                           state->mOutputPixelX,
                                                           state->mOutputPixelY);
    if (isBlack(inputNormal)) {
        *result = self->mNormalMapOutput ? Color_ctor(0.5f, 0.5f, 1.0f) : inputColor;
        return;
    }
    const varying Vec3f N = normalize(Vec3f_ctor(inputNormal.r, inputNormal.g, inputNormal.b));

    const uniform InputBuffer const * uniform dPdsBuffer = inputBuffers[2];
    const varying Color inputdPds = InputBuffer_getPixel(dPdsBuffer,
                                                         state->mOutputPixelX,
                                                         state->mOutputPixelY);
    if (isBlack(inputdPds)) {
        *result = self->mNormalMapOutput ? Color_ctor(0.5f, 0.5f, 1.0f) : inputColor;
        return;
    }
    const varying Vec3f dPds = normalize(Vec3f_ctor(inputdPds.r, inputdPds.g, inputdPds.b));

    ReferenceFrame frame;
    ReferenceFrame_init(frame, N, dPds);
    Vec3f output = globalToLocal(frame, input);

    if (self->mNormalMapOutput) {
        output = output * 0.5f + Vec3f_ctor(0.5f, 0.5f, 0.5f);
        output.x = saturate(output.x);
        output.y = saturate(output.y);
        output.z = saturate(output.z);
    }

    *result = Color_ctor(output.x, output.y, output.z);
}

DEFINE_DISPLAY_FILTER(TangentSpaceDisplayFilter, filter)

//---------------------------------------------------------------------------

