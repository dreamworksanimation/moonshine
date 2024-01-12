// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include <moonray/rendering/displayfilter/DisplayFilter.isph>

#include <scene_rdl2/common/math/ispc/Math.isph>
#include <scene_rdl2/common/math/ispc/Vec3.isph>

struct ToonDisplayFilter
{
    unsigned int mNumCels;
    Color mAmbient;
    float mInkDepthThreshold;
    float mInkNormalThreshold;
    float mInkNormalScale;
    int mEdgeDetector;
};

export const uniform ToonDisplayFilter * uniform
ToonDisplayFilter_get(const uniform DisplayFilter * uniform displayFilter)
{
    return DISPLAYFILTER_GET_ISPC_CPTR(ToonDisplayFilter, displayFilter);
}

static varying float
discretize(const varying float t, const uniform unsigned int numBins)
{
    return ceil(t * numBins) / numBins;
}

varying float
convolveDepth(const uniform float * uniform kernel,
              const uniform unsigned int kSizeSqrt, // eg. 3, 5, 7, 9, etc
              varying unsigned int x, varying unsigned int y,
              const uniform InputBuffer const * uniform inBuffer,
              const varying float depthDenom)
{
    const uniform int kRad = (kSizeSqrt -1) / 2;

    varying float result = 0.f;

    for (uniform int ky = -kRad, j = 0; ky <= kRad; ++ky, ++j) {
        for (uniform int kx = -kRad, i = 0; kx <= kRad; ++kx, ++i) {
            const uniform float wt = kernel[j * kSizeSqrt + i];
            const varying float src = InputBuffer_getFloatPixel(inBuffer, x + kx, y + ky);
            result = result + src * wt * depthDenom;
        }
    }
    return result;
}

varying float
convolveNormal(const uniform float * uniform kernel,
               const uniform unsigned int kSizeSqrt, // eg. 3, 5, 7, 9, etc
               varying unsigned int x, varying unsigned int y,
               const uniform InputBuffer const * uniform inBuffer,
               const varying Vec3f normal)
{
    const uniform int kRad = (kSizeSqrt -1) / 2;

    varying float result = 0.f;

    for (uniform int ky = -kRad, j = 0; ky <= kRad; ++ky, ++j) {
        for (uniform int kx = -kRad, i = 0; kx <= kRad; ++kx, ++i) {
            const uniform float wt = kernel[j * kSizeSqrt + i];
            const varying Vec3f src = InputBuffer_getFloat3Pixel(inBuffer, x + kx, y + ky);
            result = result + dot(src, normal) * wt;
        }
    }
    return result;
}

static void
filter(const uniform DisplayFilter * uniform me,
       const uniform InputBuffer * const uniform * const uniform inputBuffers,
       const varying DisplayFilterState * const uniform state,
       varying Color * uniform output)
{
    const uniform ToonDisplayFilter * uniform self = ToonDisplayFilter_get(me);

    // input buffers given to us are in the same order as the
    // inputs SceneObjects we returned from getInputData();
    const uniform InputBuffer const * uniform inDiffuseBuffer = inputBuffers[0];
    const uniform InputBuffer const * uniform inGlossyBuffer = inputBuffers[1];
    const uniform InputBuffer const * uniform inAlbedoBuffer = inputBuffers[2];
    const uniform InputBuffer const * uniform inDepthBuffer = inputBuffers[3];
    const uniform InputBuffer const * uniform inNormalBuffer = inputBuffers[4];

    static uniform float laplacian5[25] = {
        -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f, 24.0f, -1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f, -1.0f, -1.0f};

    static uniform float laplacianOfGaussian[81] = {
         0.0f,   1.0f,   1.0f,   2.0f,   2.0f,   2.0f,   1.0f,   1.0f,   0.0f,
         1.0f,   2.0f,   4.0f,   5.0f,   5.0f,   5.0f,   4.0f,   2.0f,   1.0f,
         1.0f,   4.0f,   5.0f,   3.0f,   0.0f,   3.0f,   5.0f,   4.0f,   1.0f,
         2.0f,   5.0f,   3.0f, -12.0f, -24.0f, -12.0f,   3.0f,   5.0f,   2.0f,
         2.0f,   5.0f,   0.0f, -24.0f, -40.0f, -24.0f,   0.0f,   5.0f,   2.0f,
         2.0f,   5.0f,   3.0f, -12.0f, -24.0f, -12.0f,   3.0f,   5.0f,   2.0f,
         1.0f,   4.0f,   5.0f,   3.0f,   0.0f,   3.0f,   5.0f,   4.0f,   1.0f,
         1.0f,   2.0f,   4.0f,   5.0f,   5.0f,   5.0f,   4.0f,   2.0f,   1.0f,
         0.0f,   1.0f,   1.0f,   2.0f,   2.0f,   2.0f,   1.0f,   1.0f,   0.0f};

    static uniform float sobelH[9] = {
        -1.0f,  0.0f,  1.0f,
        -2.0f,  0.0f,  2.0f,
        -1.0f,  0.0f,  1.0f};

    static uniform float sobelV[9] = {
         1.0f,  2.0f,  1.0f,
         0.0f,  0.0f,  0.0f,
        -1.0f, -2.0f, -1.0f};

    const varying Vec3f diffuse = InputBuffer_getFloat3Pixel(inDiffuseBuffer,
                                                             state->mOutputPixelX,
                                                             state->mOutputPixelY);
    const varying Vec3f glossy = InputBuffer_getFloat3Pixel(inGlossyBuffer,
                                                            state->mOutputPixelX,
                                                            state->mOutputPixelY);
    const varying Vec3f albedo = InputBuffer_getFloat3Pixel(inAlbedoBuffer,
                                                            state->mOutputPixelX,
                                                            state->mOutputPixelY);

    // remove 'local' surface color
    Vec3f result = Vec3f_ctor(diffuse.x, diffuse.y, diffuse.z);
    result.x /= (isZero(albedo.x) ? 1.0f : albedo.x);
    result.y /= (isZero(albedo.y) ? 1.0f : albedo.y);
    result.z /= (isZero(albedo.z) ? 1.0f : albedo.z);

    // find max component, we'll use this to compute a discretized
    // value.
    const float maxComp = max(result.x, max(result.y, result.z));
    float cel = discretize(maxComp, self->mNumCels);

    // We then compute a scale factor to be applied to
    // all three channels to preserve hue and saturation
    const float scale = isZero(maxComp) ? 0.f : (cel/maxComp);

    // scale all channels equally to maintain the lights' hue/sat while
    // creating cel-shading effect via value
    result.x *= scale;
    result.y *= scale;
    result.z *= scale;

    // add "ambient" light
    const uniform Color ambient = self->mAmbient;
    result.x += ambient.r;
    result.y += ambient.g;
    result.z += ambient.b;

    // restore "local" surface color
    result = result * albedo;

    // add glossy term - do nothing to it for now
    result = result + glossy;

    // depth-based edge detection
    const varying float depth = InputBuffer_getFloatPixel(inDepthBuffer,
                                                          state->mOutputPixelX,
                                                          state->mOutputPixelY);
    const float depthDenom = 1.0f / (isZero(depth) ? 1.0f : depth);
    float edgeD = 0.0f;
    switch(self->mEdgeDetector) {
    case 1:
        {
            // Sobel edge detector
            const float Gx = convolveDepth(sobelH, 3,
                                           state->mOutputPixelX, state->mOutputPixelY,
                                           inDepthBuffer,
                                           depthDenom);

            const float Gy = convolveDepth(sobelV, 3,
                                           state->mOutputPixelX, state->mOutputPixelY,
                                           inDepthBuffer,
                                           depthDenom);

            edgeD = sqrt(Gx*Gx + Gy*Gy);
            break;
        }
    case 2:
        {
            // Laplacian edge detector
            edgeD = convolveDepth(laplacian5, 5,
                                  state->mOutputPixelX, state->mOutputPixelY,
                                  inDepthBuffer,
                                  depthDenom);
            break;
        }
    case 3:
        {
            // Laplacian of Gaussian edge detector
            edgeD = convolveDepth(laplacianOfGaussian, 9,
                                  state->mOutputPixelX, state->mOutputPixelY,
                                  inDepthBuffer,
                                  depthDenom);
            break;
        }
    case 0:
    default:
        break;
    }

    const varying Vec3f normal = InputBuffer_getFloat3Pixel(inNormalBuffer,
                                                            state->mOutputPixelX,
                                                            state->mOutputPixelY);
    float edgeN = 0.0f;
    if (!isZero(normal.x) && !isZero(normal.y) && !isZero(normal.z)) {
        switch (self->mEdgeDetector) {
        case 1:
            {
                // Sobel edge detector
                const float Gx = convolveNormal(sobelH, 3,
                                                state->mOutputPixelX, state->mOutputPixelY,
                                                inNormalBuffer, normal);

                const float Gy = convolveNormal(sobelV, 3,
                                                state->mOutputPixelX, state->mOutputPixelY,
                                                inNormalBuffer, normal);

                edgeN = sqrt(Gx*Gx + Gy*Gy);
                break;
            }
        case 2:
            {
                // Laplacian edge detector
                edgeN = convolveNormal(laplacian5, 5,
                                       state->mOutputPixelX, state->mOutputPixelY,
                                       inNormalBuffer, normal);
                break;
            }
        case 3:
            {
                // Laplacian of Gaussian edge detector
                edgeN = convolveNormal(laplacianOfGaussian, 9,
                                       state->mOutputPixelX, state->mOutputPixelY,
                                       inNormalBuffer, normal);
                break;
            }
        case 0:
        default:
            break;
        }
    }

    edgeD = edgeD - self->mInkDepthThreshold;
    const float inkD = 1.0f - saturate(edgeD);

    edgeN = edgeN - self->mInkNormalThreshold;
    const float inkN = 1.0f - saturate(edgeN * self->mInkNormalScale);

    result = result * min(inkD, inkN);

    *output = Color_ctor(result.x, result.y, result.z);
}

DEFINE_DISPLAY_FILTER(ToonDisplayFilter, filter)

//---------------------------------------------------------------------------

