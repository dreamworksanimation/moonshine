// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file Glitter.ispc

#include "Glitter.isph"
#include "HueVariationTables.isph"

#include <moonshine/map/projection/ispc/Projection.isph>
#include <moonray/map/primvar/ispc/Primvar.isph>

#include <scene_rdl2/common/math/ispc/ColorSpace.isph>
#include <scene_rdl2/render/util/Arena.isph>
#include <scene_rdl2/scene/rdl2/rdl2.isph>
#include <scene_rdl2/common/platform/Platform.isph>
#include <moonshine/common/interpolation/ispc/Interpolation.isph>

// Below 3 values are hardcoded - feel free to change for debugging / testing
#define MACROFLAKES_FULL_THRESHOLD 0.99f // If macroflakes cover 99% of footprint, no other LOD modes done
#define MACROFLAKES_MAX_THRESHOLD 0.9f   // If macroflakes cover > 90% of footprint, approximate micro flakes
#define MACROFLAKES_MIN_THRESHOLD 0.1f   // If macroflakes cover < 10% of footprint, don't do first LOD

#define GLITTERFLAKES_MIN_BLEND_START 100
#define GLITTERFLAKES_MAX_BLEND_START 900
#define GLITTERFLAKES_BLEND_START_RANGE (GLITTERFLAKES_MAX_BLEND_START - GLITTERFLAKES_MIN_BLEND_START)

#define MACROFLAKES_MAX_COUNT 4

ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(GLITTER_StaticData);
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(GLITTER_UniformParameters);
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(GLITTER_VaryingParameters);
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(GLITTER_Glitter);
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(GLITTER_DebugModes);
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(GLITTER_ResultCode);
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(GLITTER_LayeringModes);

static const unsigned int sMaxMacroFlakeCount = MACROFLAKES_MAX_COUNT;

//  This function computes the fraction of the pixel footprint that is covered on average
//  by glitter flakes, given a flake size.
//  The numerous seemingly magic numbers below are a result of fitting a curve to the
//  data obtained by analysing the average coverage at various flake sizes.
//  See /work/gshad/moonshine/glitterFlake/flake_coverage_analysis/glitterplot.png for the curve fit.
//  The curve is broken down into two parts, a quadratic curve from 0.0 to 0.4 flake size,
//  and a cubic curve from 0.4 to 1.0 flake size (both obtained by solving a linear system of equations
//  from picking points of inflection on the curve).
//
//  **More info** - the analysis uses a scene that is a unit plane centered at origin, with a camera
//  looking straight down. At each flake size, multiple images are rendered of this plane using different
//  seeds for the glitter flake distribution. The 'image_coverage_stats' utility in the moonshine folio
//  is used to obtain the min, max and mean of the coverage for each flake size and this is what is
//  plotted out for curve fitting (see stats.txt for data, plot.p for the gnuplot description).
inline varying float
computeCoverageFactor(float flakeSize)
{
    // Clamp at high inflection point
    // See desmos graph: https://www.desmos.com/calculator/44kglnsfgy
    flakeSize = min(flakeSize, 1.1277f);

    float res = 0.0f;
    const float flakeSizeSq = flakeSize * flakeSize;
    if (flakeSize < 0.4f) {
        res = (-0.0382308f * flakeSize) + (1.230008f * flakeSizeSq);
    } else {
        res = 0.030076f - (0.6612578f * flakeSize) + (3.313868f * flakeSizeSq)
                - (1.7856762f * flakeSize*flakeSizeSq);
    }
    return saturate(res);
}

inline varying Color
applyColorVariationInHsv(const varying Color& baseColor,
                         const varying Vec3f& hsvVariation,
                         const varying Vec3f& randoms)
{
    float h = baseColor.r;
    float s = baseColor.g;
    float v = baseColor.b;

    // Hue variation is applied around the hue of the base flake color equally in clockwise and
    // anti-clockwise direction.
    // Saturation and Value variation is +/- variation clamped to [0, 1] range
    h += hsvVariation.x * randoms.x;
    h = h - floor(h); // Wrap Hue

    s += hsvVariation.y * randoms.y;
    s = saturate(s);

    v += hsvVariation.z * randoms.z;
    v = saturate(v);

    return Color_ctor(h, s, v);
}

bool
calculateDeformationFactors(uniform ShadingTLState *uniform  tls,
                            const varying State& state,
                            const uniform GLITTER_Glitter * uniform me,
                            const varying bool compensateDeformation,
                            varying NOISE_WorleySample& sample,
                            const uniform GLITTER_UniformParameters * uniform uParams,
                            GLITTER_ResultCode& resultCode)
{
    if (!compensateDeformation || uParams->mSpace != SHADING_SPACE_REFERENCE) {
        return false;
    }
    // Current Space Vectors
    const Vec3f curdPds = getdPds(state);
    const Vec3f curdPdt = getdPdt(state);

    Vec3f curZ = cross(curdPds, curdPdt);
    const float lengthCurZ = length(curZ);
    // Returns false if either curdPds or curdPdt are zero vectors or if they are identical.
    if (isZero(lengthCurZ)) {
        return false;
    }
    curZ = curZ / lengthCurZ;
    const Vec3f curX = normalize(curdPds);
    const Vec3f curY = normalize(cross(curZ, curX));

    // Reference Space Vectors
    Vec3f refdPds, refdPdt;
    refdPds = getdVec3fAttributeds(tls, state, me->mRefPKey);
    refdPdt = getdVec3fAttributedt(tls, state, me->mRefPKey);

    Vec3f refZ = cross(refdPds, refdPdt);
    const float lengthRefZ = length(refZ);
    // Returns false if either refdPds or refdPdt are zero vectors or if they are identical.
    if (isZero(lengthRefZ)) {
        resultCode = GLITTER_RESULTCODE_NO_REFP_PARTIALS;
        return false;
    }
    refZ = refZ / lengthRefZ;
    const Vec3f refX = normalize(refdPds);
    const Vec3f refY = normalize(cross(refZ, refX));

    // Stretch/Compression Factors
    const float avgStretchC = abs(dot(curdPds, curX) / dot(refdPds, refX));
    const float avgStretchR = abs(dot(curdPdt, curY) / dot(refdPdt, refY));
    sample.compensationS = avgStretchC;
    sample.compensationT = avgStretchR;

    // Shear Factors along X Axis
    const float shearCurP_X = dot(normalize(curdPdt), curX);
    const float shearRefP_X = dot(normalize(refdPdt), refX);
    sample.shearRefP_X = shearRefP_X - shearCurP_X;

    // Reference Space Axes
    sample.refX = refX;
    sample.refY = refY;
    sample.refZ = refZ;

    return true;
}

bool
initializeNoiseSample(varying NOISE_WorleySample& noiseSample,
                      uniform ShadingTLState *uniform  tls,
                      const varying State& state,
                      const uniform GLITTER_Glitter * uniform me,
                      const uniform GLITTER_UniformParameters * uniform uParams,
                      const varying GLITTER_VaryingParameters& vParams,
                      GLITTER_ResultCode& resultCode)
{
    uniform SHADING_Space returnSpace = uParams->mSpace;

    uniform int inputSourceMode;
    if (uParams->mSpace == SHADING_SPACE_REFERENCE) {
        inputSourceMode = INPUT_SOURCE_MODE_REF_P_REF_N;
    } else {
        inputSourceMode = INPUT_SOURCE_MODE_P_N;
    }

    varying Vec3f pos, pos_ddx, pos_ddy, pos_ddz, inputPosition;
    if (!PRIMVAR_getPosition(tls, state,
                             inputSourceMode,
                             inputPosition,
                             me->mXform,
                             returnSpace,
                             me->mRefPKey,
                             pos, pos_ddx, pos_ddy, pos_ddz)) {
        resultCode = GLITTER_RESULTCODE_NO_REFP;
        return false;
    }

    varying Vec3f normal, inputNormal;
    if (!PRIMVAR_getNormal(tls, state,
                           inputSourceMode,
                           inputNormal,
                           me->mXform,
                           returnSpace,
                           me->mRefPKey,
                           me->mRefNKey,
                           normal)) {
        resultCode = GLITTER_RESULTCODE_NO_REFN;
        return false;
    }

    // Ensure the Glitter Normal is Normalized
    noiseSample.normal = normalize(normal);

    noiseSample.position = pos;

    Vec3f dPds, dPdt;
    if (returnSpace == SHADING_SPACE_REFERENCE) {
        dPds = state.mdPds;
        dPdt = state.mdPdt;
    } else {
        dPds = transformVector(me->mXform,
                               SHADING_SPACE_RENDER,
                               returnSpace,
                               state,
                               state.mdPds);
        dPdt = transformVector(me->mXform,
                               SHADING_SPACE_RENDER,
                               returnSpace,
                               state,
                               state.mdPdt);
    }

    dPds = dPds * vParams.mFlakeDensity;
    dPdt = dPdt * vParams.mFlakeDensity;
    Vec3f dPdxScaled = dPds * state.mdSdx + dPdt * state.mdTdx;
    Vec3f dPdyScaled = dPds * state.mdSdy + dPdt * state.mdTdy;

    noiseSample.position = noiseSample.position * vParams.mFlakeDensity;
    noiseSample.dNdx = Vec3f_ctor(0.0f);
    noiseSample.dNdy = Vec3f_ctor(0.0f);
    noiseSample.footprintArea = length(cross(dPdxScaled, dPdyScaled));
    noiseSample.radius = sqrt(noiseSample.footprintArea * sOneOverPi);
    noiseSample.footprintLength = 2.0f * noiseSample.radius;
    noiseSample.estimatedFeatures = (int)(NOISE_WORLEY_LAMBDA * noiseSample.footprintArea + 1);

    // mSearchRadiusFactor controls how much of the calculated radius is used
    // to search for worley points (0..1).   Larger values produce more stable
    // results at the cost of speed.   However, value of 0.25 is a good compromise
    // that is nearly indistinguishable from a value of 1.0.
    // This animation shows an animated wedge of mSearchRadiusFactor:
    // r_play /work/gshad/moonshine_test/glitterFlake/searchRadius/images/out_txt.*.exr
    noiseSample.searchRadius = noiseSample.radius * uParams->mSearchRadiusFactor;

    // Initialize blending factor between microflake and microfacet modes
    noiseSample.microfacetBlend = saturate((float)((int)noiseSample.estimatedFeatures - me->mMicrofacetBlendStart) /
                                           (float)(me->mMicrofacetBlendEnd - me->mMicrofacetBlendStart));

    noiseSample.compensateDeformation = calculateDeformationFactors(tls,
                                                                    state,
                                                                    me,
                                                                    vParams.mCompensateDeformation,
                                                                    noiseSample,
                                                                    uParams,
                                                                    resultCode);

    return true;
}

bool cmpByLesserID(const varying NOISE_WorleyPoint * varying x,
                   const varying NOISE_WorleyPoint * varying y)
{
    return x->id < y->id;
}

varying unsigned int 
finalizeFlakes(varying NOISE_WorleyPoint *uniform flakeArray,
               const varying int flkBegIndex,
               const varying int flkCurrIndex)
{
    const unsigned int flakeCount = flkCurrIndex - flkBegIndex;

    // The flake size is independent of flake density
    // so we need to sort all of the flakes even if the flake size is less than one.
    NOISE_worleySortPoints(flakeArray, 0, flkCurrIndex - 1, &cmpByLesserID);
    return flakeCount;
}

varying unsigned int
findNearestFlakes(const uniform GLITTER_Glitter * uniform me,
                  uniform ShadingTLState* uniform tls,
                  const uniform GLITTER_UniformParameters * uniform uParams,
                  const varying NOISE_WorleySample& sample,
                  const varying float (&flakeStyleCDF)[NOISE_WORLEY_GLITTER_NUM_STYLES],
                  const varying float (&flakeStyleSizes)[NOISE_WORLEY_GLITTER_NUM_STYLES],
                  const varying float flakeDensity,
                  const varying float flakeJitter,
                  varying NOISE_WorleyPoint *uniform flakeArray)
{
    varying float flakeSize0 = flakeStyleSizes[0] * 0.5f;
    varying float flakeSize1 = flakeStyleSizes[1] * 0.5f;
    flakeSize0 = flakeSize0 * flakeDensity;
    flakeSize1 = flakeSize1 * flakeDensity;

    const varying float searchRadius[NOISE_WORLEY_GLITTER_NUM_STYLES] = {flakeSize0, flakeSize1};
    const varying float (*searchRadiusPtr)[NOISE_WORLEY_GLITTER_NUM_STYLES] = &searchRadius;
    const uniform float (*randomTablePtr)[NOISE_WORLEY_GLITTER_TABLE_SIZE] = &(me->mRandomTable);
    const varying float (*styleCDFPtr)[NOISE_WORLEY_GLITTER_NUM_STYLES] = &flakeStyleCDF;

    varying int flkCurrIndex = NOISE_worleySearchPoints(me->mNoiseWorley,
                                                        tls,
                                                        searchRadiusPtr,
                                                        randomTablePtr,
                                                        styleCDFPtr,
                                                        sample,
                                                        flakeArray,
                                                        -1,
                                                        flakeJitter);

    return finalizeFlakes(flakeArray, 0, flkCurrIndex);
}

varying Color
computeFlakeColor(const uniform GLITTER_Glitter * uniform me,
                  const uniform GLITTER_UniformParameters * uniform uParams,
                  const varying int flakeID,
                  const varying Color& baseColor,
                  const varying Vec3f& colorVariation)
{
    // Early Exit
    if (isZero(colorVariation)) {
        return baseColor;
    }

    Vec3f randoms;
    const uniform int tabMask = NOISE_WORLEY_GLITTER_TABLE_SIZE - 1;

    // Generate 3 random numbers, one each for variation in Hue, Saturation and Value
    randoms.x = me->mRandomTable[(flakeID + 0) % tabMask] - 0.5f;          // [-0.5, 0.5]
    randoms.y = me->mRandomTable[(flakeID + 1) % tabMask] * 2.0f - 1.0f;   // [-1, 1]
    randoms.z = me->mRandomTable[(flakeID + 2) % tabMask] * 2.0f - 1.0f;   // [-1, 1]

    const Color hsv = rgbToHsv(baseColor);
    const Color modColor = applyColorVariationInHsv(hsv, colorVariation, randoms);
    return hsvToRgb(modColor);
}

// If the flake colors are varying, find the average color that represents an approximation
// by indexing into precomputed LUTs for average saturation and value which were generated
// by plotting ground truth average color for 16 base color hues (rows) and 100 hue variations (columns).
// See ispc/HueVariationTables.isph for more detail
// NOTE: Scaling for mCoverageFactor is baked into the LUT and otherwise accounted for by
// this function so there is no need to scale the result by it.
varying Color
computeAverageFlakeColor(const uniform GLITTER_Glitter * uniform me,
                         const varying Color& baseColor,
                         const varying Vec3f& hsvVariation,
                         const varying float coverageFactor)
{
    if (isZero(hsvVariation)) {
        return baseColor;
    }

    const Color baseColorHsv = rgbToHsv(baseColor);
    const float hueVar = hsvVariation.x;
    const float origHue = baseColorHsv.r;
    const float origSat = baseColorHsv.g;
    const float origVal = baseColorHsv.b;

    // The plot of how the average color changes as the hue variation increases
    // is non-linear and varies depending on the original hue.  The way the plot
    // changes shows a recurring pattern that ping pongs three times across
    // the spectrum which is represented by the origHueT variable below.
    float origHueT = fmod(origHue, 0.3333f) / 0.3333f;
    origHueT = 2.0f * ((origHueT > 0.5f) ? 1.0f - origHueT : origHueT);

    const float hueIndexFloat = origHueT * 15.0f;
    const int hueIndexLo = (int)(hueIndexFloat);
    const int hueIndexHi = (int)(min(15.0f, floor(hueIndexFloat) + 1.0f));

    const float hueVarIndexFloat = hueVar * 99.0f;
    const int hueVarIndexLo = (int)(hueVarIndexFloat);
    const int hueVarIndexHi = (int)(min(99.0f, floor(hueVarIndexFloat) + 1.0f));

    const float hueT = hueIndexFloat - (float)(hueIndexLo);
    const float varT = hueVarIndexFloat - (float)(hueVarIndexLo);

    const float s0 = GLITTER_gatherHueVariationSaturation(hueIndexLo, hueVarIndexLo);
    const float s1 = GLITTER_gatherHueVariationSaturation(hueIndexLo, hueVarIndexHi);
    const float s2 = GLITTER_gatherHueVariationSaturation(hueIndexHi, hueVarIndexLo);
    const float s3 = GLITTER_gatherHueVariationSaturation(hueIndexHi, hueVarIndexHi);
    const float avgSat = origSat * lerp(lerp(s0, s1, varT), lerp(s2, s3, varT), hueT);

    const float v0 = GLITTER_gatherHueVariationValue(hueIndexLo, hueVarIndexLo);
    const float v1 = GLITTER_gatherHueVariationValue(hueIndexLo, hueVarIndexHi);
    const float v2 = GLITTER_gatherHueVariationValue(hueIndexHi, hueVarIndexLo);
    const float v3 = GLITTER_gatherHueVariationValue(hueIndexHi, hueVarIndexHi);
    float avgVal = origVal * lerp(lerp(v0, v1, varT), lerp(v2, v3, varT), hueT);
    avgVal = lerp(origVal * coverageFactor, avgVal, origSat);

    return hsvToRgb(Color_ctor(origHue, avgSat, avgVal));
}

varying Color
computeAverageBaseColor(const varying GLITTER_VaryingParameters& vParams)
{
    // Computes the average color of the flakes when there are multiple styles,
    // for LOD purposes.
    // Takes the size and frequency of each flake style into account
    // and lerps the colors in HSV space.
    if (isZero(vParams.mFlakeStyleFrequency[0])) {
        return vParams.mFlakeColor[1];
    }
    if (isZero(vParams.mFlakeStyleFrequency[1])) {
        return vParams.mFlakeColor[0];
    }
    const float size1Sqr = vParams.mFlakeSize[0] * vParams.mFlakeSize[0];
    const float size2Sqr = vParams.mFlakeSize[1] * vParams.mFlakeSize[1];
    const float size1Ratio = size1Sqr / (size1Sqr + size2Sqr);
    const float size2Ratio = size2Sqr / (size1Sqr + size2Sqr);
    const float flake1Proportion = size1Ratio * vParams.mFlakeStyleFrequency[0];
    const float flake2Proportion = size2Ratio * vParams.mFlakeStyleFrequency[1];
    const float t = flake2Proportion / (flake1Proportion + flake2Proportion);
    const Color color1 = vParams.mFlakeColor[0];
    const Color color2 = vParams.mFlakeColor[1];
    const Color flakeColor1HSV = rgbToHsv(color1);
    const Color flakeColor2HSV = rgbToHsv(color2);
    const Color averageFlakeColorHSV = INTERPOLATION_lerpHSV(t, flakeColor1HSV, flakeColor2HSV);
    return hsvToRgb(averageFlakeColorHSV);
}

int
chooseFlakePattern(const uniform GLITTER_Glitter * uniform me,
                   varying int id)
{
    const uniform int tabMask = NOISE_WORLEY_GLITTER_TABLE_SIZE - 1;
    // Choose a Random Number
    const varying float r1 = me->mRandomTable[(7*id) & tabMask];      // [0, 1]
    if (r1 <= me->mFlakeTextureCDF[0]) {
        return 0;
    } else if (r1 <= me->mFlakeTextureCDF[1]) {
        return 1;
    }
    // Regular Circular Flakes
    return -1;
}

void
rotateFlakeUVs(const uniform GLITTER_Glitter * uniform me,
               const varying float flakeOrientationRandomness,
               varying int id,
               varying Vec2f& uv)
{
    const uniform int tabMask = NOISE_WORLEY_GLITTER_TABLE_SIZE - 1;
    // Choose a Random Number
    varying float r1 = me->mRandomTable[(29*id) & tabMask];      // [0, 1]
    // [-0.5, 0.5]
    r1 -= 0.5f;
    const float theta = r1 * sTwoPi * flakeOrientationRandomness;
    float sinTheta, cosTheta;
    sincos(theta, &sinTheta, &cosTheta);
    float newU = uv.x * cosTheta - uv.y * sinTheta;
    float newV = uv.x * sinTheta + uv.y * cosTheta;
    uv.x = newU;
    uv.y = newV;
}

void
readFlakeTexturesAndModifyWeights(const uniform GLITTER_Glitter * uniform me,
                                  const uniform GLITTER_UniformParameters * uniform uParams,
                                  uniform ShadingTLState* uniform tls,
                                  const varying State& state,
                                  const unsigned int macroFlakeCount,
                                  const varying float flakeOrientationRandomness,
                                  varying NOISE_WorleyPoint *uniform flakeArray,
                                  Color (&flakeTextures)[MACROFLAKES_MAX_COUNT])
{
    varying float derivatives[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    for (unsigned int i = 0; i < macroFlakeCount; ++i) {
        // default white
        flakeTextures[i] = sWhite;

        varying int patternIndex = chooseFlakePattern(me,
                                                      flakeArray[i].id);
        // Do we have a valid index
        if (patternIndex >= 0 && me->mFlakeTextureData[patternIndex] != nullptr) {
            varying Vec2f uv = flakeArray[i].uv;
            if (!isZero(flakeOrientationRandomness)) {
                rotateFlakeUVs(me, flakeOrientationRandomness,
                               flakeArray[i].id,
                               uv);
            }
            // translate from [-1.0, 1.0] to [0.0, 1.0]
            uv.x = (uv.x + 1.0f) * 0.5f;
            uv.y = (uv.y + 1.0f) * 0.5f;
            Col4f t;
            // using mFlakeTextureData[patternIndex] does not compile
            // hence using explicit pointers below
            if (patternIndex == 0) {
                t = BASIC_TEXTURE_sample(me->mFlakeTextureData[0],
                                         tls,
                                         state,
                                         uv,
                                         derivatives);
            } else if (patternIndex == 1) {
                t = BASIC_TEXTURE_sample(me->mFlakeTextureData[1],
                                         tls,
                                         state,
                                         uv,
                                         derivatives);
            }
            flakeTextures[i] = Color_ctor(t.r, t.g, t.b);
            flakeArray[i].weight *= luminance(flakeTextures[i]);
        }
    }
}

void
computeStyleCDF(const varying float (&flakeStyleFrequencies)[NOISE_WORLEY_GLITTER_NUM_STYLES],
                varying float (&flakeStyleCDF)[NOISE_WORLEY_GLITTER_NUM_STYLES])
{
    float styleWeight = 0.0f;
    for (size_t i = 0; i < NOISE_WORLEY_GLITTER_NUM_STYLES; i++) {
        flakeStyleCDF[i] = flakeStyleFrequencies[i] + styleWeight;
        styleWeight += flakeStyleFrequencies[i];
    }
    if (styleWeight > 1.0f) {
        for (size_t i = 0; i < NOISE_WORLEY_GLITTER_NUM_STYLES; i++) {
            flakeStyleCDF[i] /= styleWeight;
        }
    }
}

void
setFlakeStyles(const uniform GLITTER_UniformParameters * uniform uParams,
               const varying GLITTER_VaryingParameters& vParams,
               const unsigned int macroFlakeCount,
               varying NOISE_WorleyPoint *uniform flakeArray,
               varying Color (&flakeColors)[MACROFLAKES_MAX_COUNT],
               varying float (&flakeRoughnesses)[MACROFLAKES_MAX_COUNT])
{
    for (unsigned int i = 0; i < macroFlakeCount; ++i) {
        int styleIndex = flakeArray[i].styleIndex;
        // Do we have a valid index
        if (styleIndex >= 0) {
            flakeColors[i] = vParams.mFlakeColor[styleIndex];
            flakeRoughnesses[i] = vParams.mFlakeRoughness[styleIndex];
        } else {
            flakeColors[i] = sBlack;
            flakeRoughnesses[i] = 0.0f;
        }
    }
}

// Add up to 4 macro flakes.
// If only 1 flake covers entire pixel, set singleMacroFlake to true
Color
createMacroFlakes(const uniform GLITTER_Glitter * uniform me,
                  varying BsdfBuilder& bsdfBuilder,
                  const uniform GLITTER_UniformParameters * uniform uParams,
                  uniform ShadingTLState* uniform tls,
                  const varying State& state,
                  const varying int macroFlakeCount,
                  const varying int totalFlakeCount,
                  const varying float macroFlakeVisibility,
                  const varying ReferenceFrame& refFrame,
                  varying NOISE_WorleyPoint *uniform flakeArray,
                  const varying Color (&baseColor)[MACROFLAKES_MAX_COUNT],
                  const varying float (&flakeRoughnesses)[MACROFLAKES_MAX_COUNT],
                  Color (&flakeTextures)[MACROFLAKES_MAX_COUNT],
                  const varying Vec3f& colorVariation,
                  const varying float mask,
                  const uniform int label,
                  const varying float coverageFactor,
                  varying BsdfBuilderBehavior combineBehavior,
                  varying bool& singleMacroFlake)
{
    // assume we'll only create one for now
    singleMacroFlake = true;

    Color accumMacroFlakesColor = sBlack;

    // Implementing a crude version of std::find_if
    varying int first = 0;
    varying int last = macroFlakeCount;
    while ((first != last) && !(flakeArray[first].weight >= sOneMinusEpsilon)) {
        ++first;
    }

    // If we are close enough to the surface that there's only one glitter flake in the ray
    // footprint, or, if more flakes are present with one flake completely covering the ray footprint,
    // add only one ggx lobe representing this flake for the shading point.
    Color currFlakeColor = sBlack;
    Vec3f currNormal;
    float currFlakeRoughness;
    float flakeWeight = 1.0f;
    if (macroFlakeCount == 1) {
        Color flakeColor = baseColor[0] * flakeTextures[0];
        currFlakeColor = computeFlakeColor(me, uParams, flakeArray[0].id, flakeColor, colorVariation);
        currNormal = localToGlobal(refFrame, flakeArray[0].normal);
        currFlakeRoughness = flakeRoughnesses[0];
        flakeWeight = flakeArray[0].weight;
    } else if (first < macroFlakeCount) { // found flake with > 1 coverage
        Color flakeColor = baseColor[first] * flakeTextures[first];
        currFlakeColor = computeFlakeColor(me, uParams, flakeArray[first].id, flakeColor, colorVariation);
        currNormal = localToGlobal(refFrame, flakeArray[first].normal);
        currFlakeRoughness = flakeRoughnesses[first];
        flakeWeight = flakeArray[first].weight;
    }

    // If either of the two conditions above are true (1 flake with full pixel coverage), then currFlakeColor
    // is initialized to a non-black flake color - add one ggx lobe for that flake and we're done shading
    if (!isBlack(currFlakeColor)) {
        GlitterFlakeBRDF glitterFlakeRefl;
        GlitterFlakeBRDF_init(glitterFlakeRefl,
                              getN(refFrame),
                              currNormal,
                              currFlakeColor,
                              currFlakeColor,
                              currFlakeRoughness);

        BsdfBuilder_addGlitterFlakeBRDF(bsdfBuilder,
                                        glitterFlakeRefl,
                                        mask * flakeWeight,
                                        combineBehavior,
                                        label);

        accumMacroFlakesColor = currFlakeColor;
        return accumMacroFlakesColor;
    }

    const float oneOverMacroFlakeVisibility = rcp(macroFlakeVisibility);

    // more than one flake contributes to the shading point's result
    singleMacroFlake = false;

    for (int i = 0; i < macroFlakeCount; ++i) {
        currNormal = localToGlobal(refFrame, flakeArray[i].normal);
        Color flakeColor = baseColor[i] * flakeTextures[i];
        currFlakeColor = computeFlakeColor(me, uParams, flakeArray[i].id, flakeColor, colorVariation);
        currFlakeRoughness = flakeRoughnesses[i];
        float scale = flakeArray[i].weight;

        // If there are more flakes to add after macroflakes, we're far enough away to
        // not see gaps between flakes - scale by coverage factor
        scale *= (totalFlakeCount > macroFlakeCount) ? coverageFactor : 1.0f;

        // If total visibility of the macro flakes is greater than 1.0, normalize their contribution
        if (macroFlakeVisibility >= sOneMinusEpsilon) scale *= oneOverMacroFlakeVisibility;

        // Each lobe is scaled by the "average coverage per unit area" in order to account
        // for potential overlap between the flakes
        if (!isBlack(currFlakeColor)) {
            GlitterFlakeBRDF glitterFlakeRefl;
            GlitterFlakeBRDF_init(glitterFlakeRefl,
                                  getN(refFrame),
                                  currNormal,
                                  currFlakeColor,
                                  currFlakeColor,
                                  currFlakeRoughness);

            BsdfBuilder_addGlitterFlakeBRDF(bsdfBuilder,
                                            glitterFlakeRefl,
                                            scale * mask,
                                            combineBehavior,
                                            label);

            accumMacroFlakesColor = accumMacroFlakesColor + currFlakeColor;
        }
    }

    singleMacroFlake = false;
    return accumMacroFlakesColor;
}

varying Color
createMicroFlake(const uniform GLITTER_Glitter * uniform me,
                 varying BsdfBuilder& bsdfBuilder,
                 const varying GLITTER_VaryingParameters& vParams,
                 const uniform GLITTER_UniformParameters * uniform uParams,
                 uniform ShadingTLState* uniform tls,
                 const varying State& state,
                 const varying int microFlakeCount,
                 const varying int macroFlakeCount,
                 varying NOISE_WorleyPoint *uniform flakeArray,
                 const varying ReferenceFrame& refFrame,
                 const varying float microFlakeVis,
                 const varying Color (&baseColor)[MACROFLAKES_MAX_COUNT],
                 const varying Vec3f& colorVariation,
                 const varying float mask,
                 const uniform int label,
                 const varying float coverageFactor,
                 const varying BsdfBuilderBehavior combineBehavior)
{
    Color avgColor = sBlack;

    // Microflakes are to be considered in the flake array after the first few macro flakes
    const int microFlakeOffset = macroFlakeCount;

    intptr_t flakeNormalsPtr;
    intptr_t flakeColorsPtr;

    foreach_active(lane) {
        uniform int c = extract(microFlakeCount, lane);
        flakeNormalsPtr = insert(flakeNormalsPtr, lane, (intptr_t)Arena_allocArray(tls->mArena, c, sizeof(Vec3f)));
        flakeColorsPtr = insert(flakeColorsPtr, lane, (intptr_t)Arena_allocArray(tls->mArena, c, sizeof(Color)));
    }

    varying Vec3f * varying flakeNormals = (varying Vec3f * varying)flakeNormalsPtr;
    varying Color * varying flakeColors = (varying Color * varying)flakeColorsPtr;

    // Add a microflake lobe
    if (microFlakeVis > 0.0f) {
        // Compute average color for use in both microflake and microfacet lobes
        // (populating the individual flake colors on the way)
        // Also populate the normals to pass to the StochasticFlakes Bsdf lobe
        for (int i = 0; i < microFlakeCount; ++i) {
            // modulus the (i+offset) index by macroFlakeCount to pick from the flake colors chosen for macroFlakes.
            const int flakeIndex = i + microFlakeOffset;
            const int limitedFlakeIndex = (int)fmod(flakeIndex, macroFlakeCount);
            varying Color baseColorI = baseColor[limitedFlakeIndex];
            flakeColors[i] = computeFlakeColor(me, uParams, flakeArray[flakeIndex].id,
                                                            baseColorI, colorVariation);
            avgColor = avgColor + flakeColors[i];
            flakeNormals[i] = localToGlobal(refFrame, flakeArray[flakeIndex].normal);
        }

        // TODO: The StochasticFlakes lobe can be passed an array of roughnesses
        // rather than just picking the first one.
        StochasticFlakesBRDF stochasticFlakesRefl;
        StochasticFlakesBRDF_init(stochasticFlakesRefl,
                                  getN(refFrame),
                                  (unsigned int)microFlakeCount,
                                  flakeNormals,
                                  flakeColors,
                                  vParams.mFlakeRoughness[0],
                                  uParams->mFlakeRandomness);

        BsdfBuilder_addStochasticFlakesBRDF(bsdfBuilder,
                                            stochasticFlakesRefl,
                                            microFlakeVis * coverageFactor * mask,
                                            combineBehavior,
                                            label);
    }

    return avgColor * rcp((float)(microFlakeCount));
}

// The debug mode helps visualize the contributions from each of the
// three modes that blend together.
// Red reprenents macro flake contribution
// Green represents micro flake contribution
// Blue represents ggx microfacet contribution
// The three always add up to 1.0 unless insufficient number
// of flakes are found in the ray footprint.
void
GLITTER_createDebugBlendLobes(const uniform GLITTER_Glitter * uniform me,
                              varying BsdfBuilder& bsdfBuilder,
                              uniform ShadingTLState* uniform tls,
                              const varying State& state,
                              const uniform GLITTER_UniformParameters * uniform uParams,
                              const varying GLITTER_VaryingParameters& vParams,
                              GLITTER_ResultCode& resultCode)
{
    static const varying Color sMacroFlakeDebugColor = {1.0f, 0.0f, 0.0f};
    static const varying Color sMicroFlakeDebugColor = {0.0f, 1.0f, 0.0f};
    static const varying Color sMicroFacetDebugColor = {0.0f, 0.0f, 1.0f};

    if (!isHifi(state)) {
        BsdfBuilder_addEmission(bsdfBuilder, sMicroFacetDebugColor);
        return;
    }

    varying NOISE_WorleySample sample;
    const bool isValidNoiseSample = initializeNoiseSample(sample, tls, state, me, uParams, vParams, resultCode);
    if (!isValidNoiseSample) {
        return;
    }

    if (isOne(sample.microfacetBlend)) {
        BsdfBuilder_addEmission(bsdfBuilder, sMicroFacetDebugColor);
        return;
    }

    varying float flakeStyleCDF[NOISE_WORLEY_GLITTER_NUM_STYLES];
    computeStyleCDF(vParams.mFlakeStyleFrequency, flakeStyleCDF);

    // Gather flakes around the shading point, with the nearest 10 flakes ordered by ID
    uniform Arena * uniform arena = tls->mArena;
    varying NOISE_WorleyPoint *uniform flakes = (varying NOISE_WorleyPoint *uniform)
        Arena_allocArray(arena, NOISE_WORLEY_MAX_SEARCH_POINTS, sizeof(varying NOISE_WorleyPoint));

    const unsigned int flakeCount = findNearestFlakes(me,
                                                      tls,
                                                      uParams,
                                                      sample,
                                                      flakeStyleCDF,
                                                      vParams.mFlakeSize,
                                                      vParams.mFlakeDensity,
                                                      vParams.mFlakeJitter,
                                                      flakes);

    // Early exit
    if (flakeCount == 0) {
        return;
    }

    const unsigned int macroFlakeCount = min(flakeCount, sMaxMacroFlakeCount);

    // Gather the total visibility of the first 4 flakes
    float macroFlakesVis = 0.0f;
    for (int i = 0; i < macroFlakeCount; ++i) {
        macroFlakesVis += flakes[i].weight;
    }

    if (!(macroFlakesVis > MACROFLAKES_MIN_THRESHOLD)) {
        macroFlakesVis = 0.0f;
    }

    const unsigned int microFlakeCount = flakeCount - macroFlakeCount;

    const float microFlakeVis = (microFlakeCount > 0) ? (1.0f - sample.microfacetBlend) * (1.0f - macroFlakesVis) : 0.f;
    const float microFacetVis = (microFlakeCount > 0) ? sample.microfacetBlend * (1.0f - macroFlakesVis) : 0.f;

    BsdfBuilder_addEmission(bsdfBuilder,
                            saturate(macroFlakesVis) * sMacroFlakeDebugColor +
                            saturate(microFlakeVis)  * sMicroFlakeDebugColor +
                            saturate(microFacetVis)  * sMicroFacetDebugColor);
}

void
GLITTER_createDebugColorLobes(const uniform GLITTER_Glitter * uniform me,
                              varying BsdfBuilder& bsdfBuilder,
                              uniform ShadingTLState * uniform tls,
                              const varying State& state,
                              const uniform GLITTER_UniformParameters * uniform uParams,
                              const varying GLITTER_VaryingParameters& vParams,
                              GLITTER_ResultCode& resultCode)
{
    Color emissionColor = sBlack;
    const Vec3f colorVariation = vParams.mFlakeHSVColorVariation;
    Color macroFlakesAvgColor = sBlack;
    Color currFlakeColor = sBlack;

    float coverageFactor = me->mCoverageFactor;
    float flakeSize = vParams.mFlakeSize[0] * vParams.mFlakeStyleFrequency[0] +
                      vParams.mFlakeSize[1] * vParams.mFlakeStyleFrequency[1];
    flakeSize = flakeSize * vParams.mFlakeDensity;
    coverageFactor = computeCoverageFactor(flakeSize);

    varying NOISE_WorleySample sample;
    const bool isValidNoiseSample = initializeNoiseSample(sample, tls, state, me, uParams, vParams, resultCode);
    if (!isValidNoiseSample) {
        return;
    }

    if (isOne(sample.microfacetBlend)) {
        Color averageFlakeColor = computeAverageBaseColor(vParams);
        averageFlakeColor = computeAverageFlakeColor(me, averageFlakeColor, colorVariation, coverageFactor);
        emissionColor = averageFlakeColor;
        BsdfBuilder_addEmission(bsdfBuilder, emissionColor);
        return;
    }

    varying float flakeStyleCDF[NOISE_WORLEY_GLITTER_NUM_STYLES];
    computeStyleCDF(vParams.mFlakeStyleFrequency, flakeStyleCDF);

    // Gather flakes around the shading point, with the nearest 10 flakes ordered by ID
    uniform Arena * uniform arena = tls->mArena;
    varying NOISE_WorleyPoint *uniform flakes = (varying NOISE_WorleyPoint *uniform)
        Arena_allocArray(arena, NOISE_WORLEY_MAX_SEARCH_POINTS, sizeof(varying NOISE_WorleyPoint));

    const unsigned int flakeCount = findNearestFlakes(me,
                                                      tls,
                                                      uParams,
                                                      sample,
                                                      flakeStyleCDF,
                                                      vParams.mFlakeSize,
                                                      vParams.mFlakeDensity,
                                                      vParams.mFlakeJitter,
                                                      flakes);

    if (flakeCount == 0) {
        return;
    }

    const unsigned int macroFlakeCount = min(flakeCount, sMaxMacroFlakeCount);

    varying Color baseColor[MACROFLAKES_MAX_COUNT];
    varying float flakeRoughnesses[MACROFLAKES_MAX_COUNT];
    setFlakeStyles(uParams,
                   vParams,
                   macroFlakeCount,
                   flakes,
                   baseColor,
                   flakeRoughnesses);

    // Gather the total visibility of the first 4 flakes
    float macroFlakeVis = 0.0f;
    for (unsigned int i = 0; i < macroFlakeCount; ++i) {
        macroFlakeVis += flakes[i].weight;
    }

    // Implementing a crude version of std::find_if
    varying int first = 0;
    varying int last = macroFlakeCount;
    while ((first != last) && !(flakes[first].weight >= sOneMinusEpsilon)) {
        ++first;
    }

    // Single macro flake
    float flakeWeight = 1.0f;
    if (macroFlakeCount == 1) {
        currFlakeColor = computeFlakeColor(me, uParams, flakes[0].id, baseColor[0], colorVariation);
        flakeWeight = flakes[0].weight;
    } else if (first < macroFlakeCount) { // found flake with > 1 coverage
        varying Color baseColorFirst = baseColor[first];
        currFlakeColor = computeFlakeColor(me, uParams, flakes[first].id, baseColorFirst, colorVariation);
        flakeWeight = flakes[first].weight;
    }

    // If either of the two conditions above are true (1 flake with full pixel coverage), then currFlakeColor
    // is initialized to a non-black flake color - set emission color once for that flake and we're done shading
    if (!isBlack(currFlakeColor)) {
        emissionColor = currFlakeColor * flakeWeight;
        BsdfBuilder_addEmission(bsdfBuilder, emissionColor);
        return;
    }

    // Macro flakes
    const float oneOverMacroFlakeVisibility = rcp(macroFlakeVis);
    for (int i = 0; i < macroFlakeCount; ++i) {
        varying Color baseColorI = baseColor[i];
        currFlakeColor = computeFlakeColor(me, uParams, flakes[i].id, baseColorI, colorVariation);
        float scale = flakes[i].weight;
        scale *= (flakeCount > macroFlakeCount) ? coverageFactor : 1.0f;

        // If total visibility of the macro flakes is greater than 1.0, normalize their contribution
        if (macroFlakeVis >= sOneMinusEpsilon) scale *= oneOverMacroFlakeVisibility;

        // Each lobe is scaled by the "average coverage per unit area" in order to account
        // for potential overlap between the flakes
        if (!isBlack(currFlakeColor)) {
            emissionColor = emissionColor + currFlakeColor * scale;
            macroFlakesAvgColor = macroFlakesAvgColor + currFlakeColor;
        }

    }

    // Consider all remaining flakes for the microflake model
    const unsigned int microFlakeCount = flakeCount - macroFlakeCount;

    // Only macro flakes
    if (macroFlakeVis < 1.0f && microFlakeCount == 0) {
        emissionColor = Color_ctor(
            saturate(emissionColor.r),
            saturate(emissionColor.g),
            saturate(emissionColor.b)
        );
        BsdfBuilder_addEmission(bsdfBuilder, emissionColor);
        return;
    }

    const float microFlakeVis = saturate((1.0f - sample.microfacetBlend) * (1.0f - macroFlakeVis));
    const float microFacetVis = saturate(sample.microfacetBlend * (1.0f - macroFlakeVis));

    if (macroFlakeVis >= MACROFLAKES_MAX_THRESHOLD && microFlakeCount > 0) {
        // Macro flakes > 90%
        macroFlakesAvgColor = Color_ctor(
            macroFlakesAvgColor.r / (float)(macroFlakeCount),
            macroFlakesAvgColor.g / (float)(macroFlakeCount),
            macroFlakesAvgColor.b / (float)(macroFlakeCount)
        );
        if (macroFlakeVis < MACROFLAKES_FULL_THRESHOLD && !isBlack(macroFlakesAvgColor)) {
            // Macro flakes < 99%
            emissionColor = emissionColor + macroFlakesAvgColor * (1.0f - macroFlakeVis) * coverageFactor;
        }
    } else {
        // Macro fales < 90% so do micro flakes
        Color microFlakesAvgColor = sBlack;
        if (microFlakeVis > 0.0f) {
            // microflakes
            const unsigned int offset = macroFlakeCount;
            for (unsigned int i = 0; i < microFlakeCount; ++i) {
                // modulus the (i+offset) index by macroFlakeCount to pick from the flake colors chosen for macroFlakes.
                const int flakeIndex = i + offset;
                const int limitedFlakeIndex = (int)fmod(flakeIndex, macroFlakeCount);
                varying Color baseColorI = baseColor[limitedFlakeIndex];
                microFlakesAvgColor = microFlakesAvgColor + computeFlakeColor(me, uParams,
                    flakes[flakeIndex].id, baseColorI, colorVariation);
            }
            microFlakesAvgColor = Color_ctor(
                microFlakesAvgColor.r / microFlakeCount,
                microFlakesAvgColor.g / microFlakeCount,
                microFlakesAvgColor.b / microFlakeCount
            );
            emissionColor = emissionColor + microFlakesAvgColor * microFlakeVis * coverageFactor;
        }

        // Micro facet
        if (microFacetVis > 0.0f && !isBlack(microFlakesAvgColor)) {
            // microfacet
            emissionColor = emissionColor + microFlakesAvgColor * microFacetVis * coverageFactor;
        }
    }

    emissionColor = Color_ctor(
        saturate(emissionColor.r),
        saturate(emissionColor.g),
        saturate(emissionColor.b)
    );
    BsdfBuilder_addEmission(bsdfBuilder, emissionColor);
}

void
GLITTER_createDebugAverageColorLobes(const uniform GLITTER_Glitter * uniform me,
                                     varying BsdfBuilder& bsdfBuilder,
                                     uniform ShadingTLState* uniform tls,
                                     const varying State& state,
                                     const uniform GLITTER_UniformParameters * uniform uParams,
                                     const varying GLITTER_VaryingParameters& vParams)
{
    float coverageFactor = 1.0f;
    float flakeSize = (vParams.mFlakeSize[0] * vParams.mFlakeStyleFrequency[0] +
                       vParams.mFlakeSize[1] * vParams.mFlakeStyleFrequency[1]) *
                       vParams.mFlakeDensity;
    coverageFactor = computeCoverageFactor(flakeSize);
    Vec3f colorVariation = vParams.mFlakeHSVColorVariation;
    Color averageColor = computeAverageBaseColor(vParams);
    averageColor = computeAverageFlakeColor(me, averageColor, colorVariation, coverageFactor);
    BsdfBuilder_addEmission(bsdfBuilder, averageColor);
}

void
GLITTER_createDebugSampleLobes(const uniform GLITTER_Glitter * uniform me,
                               varying BsdfBuilder& bsdfBuilder,
                               uniform ShadingTLState* uniform tls,
                               const varying State& state,
                               const uniform GLITTER_UniformParameters * uniform uParams,
                               const varying GLITTER_VaryingParameters& vParams,
                               const int mode,
                               GLITTER_ResultCode& resultCode)
{
    varying NOISE_WorleySample sample;
    const bool isValidNoiseSample = initializeNoiseSample(sample, tls, state, me, uParams, vParams, resultCode);
    if (!isValidNoiseSample) {
        return;
    }
    if (mode == GLITTER_DEBUG_MODE_FOOTPRINT_AREA) {
        BsdfBuilder_addEmission(bsdfBuilder, Color_ctor(sample.footprintArea));
    } else  if (mode == GLITTER_DEBUG_MODE_RADIUS) {
        BsdfBuilder_addEmission(bsdfBuilder, Color_ctor(sample.radius));
    }
}

// Overview:
// This is an all in one solution for rendering large flakes (cover more than 1 pixel,
// referred to as macro flakes here) and very small flakes (100s to 1000s per pixel, referred
// to as micro flakes here). The shader seamlessly blends these two modes based on the number
// of flakes found in the pixel footprint. This varies based on the flake density and the distance
// of the shading point from the camera.
// The nearest 4 flakes to the shading point are considered macro flakes and are represented
// by modeling each flake as a separate ggx lobe, weighted by the flake's footprint coverage.
// All flakes after the first 4 are represented in the micro flake model by a single
// StochasticFlakesBsdf lobe.
// For efficiency reasons, we blend in a 3rd model with the micro flake model - a simple ggx lobe,
// when the number of flakes exceeds a certain threshold (default is a linear blend between
// 500 and 2000 flakes - and exclusively to the single ggx lobe for >2000). The idea behind this
// is that when such large number of flakes are found, the shading point is usually at a far enough
// distance from the camera that individual flake contributions are not visually perceptible.
void
createLobes(const uniform GLITTER_Glitter * uniform me,
            varying BsdfBuilder& bsdfBuilder,
            uniform ShadingTLState* uniform tls,
            const varying State& state,
            const uniform GLITTER_UniformParameters * uniform uParams,
            const varying GLITTER_VaryingParameters& vParams,
            const uniform int label,
            GLITTER_ResultCode& resultCode)
{
    resultCode = GLITTER_RESULTCODE_SUCCESS;

    const float glitterMask = vParams.mGlitterMask;

    // Early exit before doing anything with glitter
    if (isZero(glitterMask)) {
        return;
    }

    // The noise sample contains all spatial information for sampling in noise space
    varying NOISE_WorleySample sample;
    const bool isValidNoiseSample = initializeNoiseSample(sample, tls, state, me, uParams, vParams, resultCode);
    if (!isValidNoiseSample) {
        return;
    }

    // Computes the tangent frame based on reference space
    ReferenceFrame refFrame;
    ReferenceFrame_init(refFrame,
        Intersection_adaptNormal((const varying Intersection &) state, getN(state)));

    Vec3f hsvColorVariation = vParams.mFlakeHSVColorVariation;

    varying float flakeStyleCDF[NOISE_WORLEY_GLITTER_NUM_STYLES];
    computeStyleCDF(vParams.mFlakeStyleFrequency, flakeStyleCDF);

    float coverageFactor = me->mCoverageFactor;
    float flakeSize = vParams.mFlakeSize[0] * vParams.mFlakeStyleFrequency[0] +
                      vParams.mFlakeSize[1] * vParams.mFlakeStyleFrequency[1];
    flakeSize = flakeSize * vParams.mFlakeDensity;
    coverageFactor = computeCoverageFactor(flakeSize);

    // Avoid gathering flakes for non-primary rays or if we know that we're far enough away
    // for only microfacet blending to contribute to this sample
    if ((!isHifi(state) && vParams.mApproximateForSecRays) || isOne(sample.microfacetBlend)) {

        Color averageFlakeColor = computeAverageBaseColor(vParams);
        averageFlakeColor = computeAverageFlakeColor(me, averageFlakeColor, hsvColorVariation, coverageFactor);

        MicrofacetIsotropicBRDF microfacetRefl;
        MicrofacetIsotropicBRDF_init(microfacetRefl,
                                     getN(refFrame),
                                     averageFlakeColor,
                                     averageFlakeColor,
                                     uParams->mFlakeRandomness,
                                     MICROFACET_DISTRIBUTION_GGX,
                                     MICROFACET_GEOMETRIC_SMITH);

        BsdfBuilder_addMicrofacetIsotropicBRDF(bsdfBuilder,
                                               microfacetRefl,
                                               coverageFactor * glitterMask,
                                               BSDFBUILDER_PHYSICAL,
                                               label);

        return;
    }

    // For primary and mirror rays use the user specified layering mode, otherwise
    // force the layering mode to be PHYSICAL for secondary and non-mirror rays
    // to prevent potentially significant energy gain with multiple bounces,
    // especially due to our use of a cook-torrance reflection lobe to approximate
    // dense glitter for secondary rays
    const varying GLITTER_LayeringModes layeringMode = isHifi(state) ?
        (GLITTER_LayeringModes)uParams->mLayeringMode :
        GLITTER_LAYERING_MODE_PHYSICAL;

    const varying bool isAdditive = (layeringMode == GLITTER_LAYERING_MODE_ADDITIVE);

    const varying BsdfBuilderBehavior combineBehavior = isAdditive ?
                                                        BSDFBUILDER_ADDITIVE :
                                                        BSDFBUILDER_PHYSICAL;

    // Gather flakes around the shading point, with the nearest 10 flakes ordered by ID
    uniform Arena * uniform arena = tls->mArena;
    varying NOISE_WorleyPoint *uniform flakes = (varying NOISE_WorleyPoint *uniform)
        Arena_allocArray(arena, NOISE_WORLEY_MAX_SEARCH_POINTS, sizeof(varying NOISE_WorleyPoint));

    const unsigned int flakeCount = findNearestFlakes(me,
                                                      tls,
                                                      uParams,
                                                      sample,
                                                      flakeStyleCDF,
                                                      vParams.mFlakeSize,
                                                      vParams.mFlakeDensity,
                                                      vParams.mFlakeJitter,
                                                      flakes);

    // Early exit after gathering flakes - no glitter lobes added
    if (flakeCount == 0) {
        return;
    }

    unsigned int macroFlakeCount = min(flakeCount, sMaxMacroFlakeCount);

    varying Color baseFlakeColors[MACROFLAKES_MAX_COUNT];
    varying float flakeRoughnesses[MACROFLAKES_MAX_COUNT];
    setFlakeStyles(uParams,
                   vParams,
                   macroFlakeCount,
                   flakes,
                   baseFlakeColors,
                   flakeRoughnesses);

    Color flakeTextures[MACROFLAKES_MAX_COUNT];
    readFlakeTexturesAndModifyWeights(me, uParams, tls, state,
                                      macroFlakeCount,
                                      vParams.mFlakeOrientationRandomness,
                                      flakes,
                                      flakeTextures);

    // Gather the total visibility of the first 4 flakes
    float macroFlakesVis = 0.0f;
    for (int i = 0; i < macroFlakeCount; ++i) {
        macroFlakesVis += flakes[i].weight;
    }

    Color macroFlakesAvgColor = sBlack;
    bool createdSingleMacroFlake = false;

    // Create GGX lobes if the total visibility is greater than ggxMinThreshold (default 10%)
    if (macroFlakesVis > MACROFLAKES_MIN_THRESHOLD) {
        macroFlakesAvgColor =
            createMacroFlakes(me, bsdfBuilder, uParams, tls, state, macroFlakeCount, flakeCount,
                              macroFlakesVis, refFrame, flakes,
                              baseFlakeColors,flakeRoughnesses, flakeTextures,
                              hsvColorVariation, glitterMask, label,
                              coverageFactor, combineBehavior,
                              createdSingleMacroFlake);
    }

    if (createdSingleMacroFlake) {
        return;
    }

    // Exit early if there are more than one macro flake returned and their coverage
    // is almost one.
    if (macroFlakesVis >= sOneMinusEpsilon) {
        return;
    }

    // Consider all remaining flakes for the microflake model
    const unsigned int microFlakeCount = flakeCount - macroFlakeCount;

    // If there are only enough flakes for macro flakes (no more flakes left in the footprint for
    // microflakes) and they together do not cover the entire footprint, shade the under material
    // for the remaining coverage
    if (macroFlakesVis < 1.0f && microFlakeCount == 0) {
        return;
    }

    const float microFlakeVis = saturate((1.0f - sample.microfacetBlend) * (1.0f - macroFlakesVis));
    const float microFacetVis = saturate(sample.microfacetBlend * (1.0f - macroFlakesVis));

    // If the 4 GGX lobes added above cover more than a certain threshold (macro flakes max threshold (default 90%),
    // and there are more flakes found in the pixel footprint, create a GGX Cook Torrance microfacet lobe
    // for remaining visibility (1.0 is full coverage) instead of a microflake lobe which is more expensive
    if (macroFlakesVis >= MACROFLAKES_MAX_THRESHOLD) {
        if (microFlakeCount > 0) {
            macroFlakesAvgColor = macroFlakesAvgColor * rcp((float)macroFlakeCount);
            // If we're above 90%, we don't do microflakes - so the below lobe is a compensation term
            // However, we don't add any more lobes if macro flakes visibility is > mMacroFlakesFullThresh (99% default)
            if (macroFlakesVis < MACROFLAKES_FULL_THRESHOLD && !isBlack(macroFlakesAvgColor)) {
                MicrofacetIsotropicBRDF microfacetRefl;
                MicrofacetIsotropicBRDF_init(microfacetRefl,
                                             getN(refFrame),
                                             macroFlakesAvgColor,
                                             macroFlakesAvgColor,
                                             uParams->mFlakeRandomness,
                                             MICROFACET_DISTRIBUTION_GGX,
                                             MICROFACET_GEOMETRIC_SMITH);

                BsdfBuilder_addMicrofacetIsotropicBRDF(bsdfBuilder,
                                                       microfacetRefl,
                                                       (1.0f - macroFlakesVis) * coverageFactor * glitterMask,
                                                       combineBehavior,
                                                       label);
            }
        }
    } else { // ggxFlakesVis < mGGXMaxThreshold
        // If the macro flakes visibility is less than ggxMaxThreshold (default 90%), create
        // a StochasticFlakesBsdf lobe - considering it as microflake contribution
        Color avgColor = createMicroFlake(me, bsdfBuilder, vParams, uParams, tls, state,
                                          microFlakeCount, macroFlakeCount, flakes, refFrame,
                                          microFlakeVis, baseFlakeColors,
                                          hsvColorVariation, glitterMask, label,
                                          coverageFactor, combineBehavior);

        // Add a Cook-Torrance lobe weighted by the remaining blend weight
        if (microFacetVis > 0.0f && !isBlack(avgColor)) {
            MicrofacetIsotropicBRDF microfacetRefl;
            MicrofacetIsotropicBRDF_init(microfacetRefl,
                                         getN(refFrame),
                                         avgColor,
                                         avgColor,
                                         uParams->mFlakeRandomness,
                                         MICROFACET_DISTRIBUTION_GGX,
                                         MICROFACET_GEOMETRIC_SMITH);

            BsdfBuilder_addMicrofacetIsotropicBRDF(bsdfBuilder,
                                                   microfacetRefl,
                                                   microFacetVis * coverageFactor * glitterMask,
                                                   combineBehavior,
                                                   label);
        }
    }

    return;
}

void
GLITTER_createLobes(const uniform GLITTER_Glitter * uniform me,
                    varying BsdfBuilder& bsdfBuilder,
                    uniform ShadingTLState* uniform tls,
                    const varying State& state,
                    const uniform GLITTER_UniformParameters * uniform uParams,
                    const varying GLITTER_VaryingParameters& vParams,
                    const uniform int label,
                    GLITTER_ResultCode& resultCode)
{
    if (uParams->mDebugMode != GLITTER_DEBUG_MODE_OFF) {
        switch(uParams->mDebugMode) {
        case GLITTER_DEBUG_MODE_BLEND:
            GLITTER_createDebugBlendLobes(me,
                                          bsdfBuilder,
                                          tls,
                                          state,
                                          uParams,
                                          vParams,
                                          resultCode);
            break;
        case GLITTER_DEBUG_MODE_COLOR:
            GLITTER_createDebugColorLobes(me,
                                          bsdfBuilder,
                                          tls,
                                          state,
                                          uParams,
                                          vParams,
                                          resultCode);
            break;
        case GLITTER_DEBUG_MODE_AVERAGE_COLOR:
            GLITTER_createDebugAverageColorLobes(me,
                                                 bsdfBuilder,
                                                 tls,
                                                 state,
                                                 uParams,
                                                 vParams);
            break;
        default:
            GLITTER_createDebugSampleLobes(me,
                                           bsdfBuilder,
                                           tls,
                                           state,
                                           uParams,
                                           vParams,
                                           uParams->mDebugMode,
                                           resultCode);
        }
        return;
    }

    // For now, glitter lib manages its own energy distribution, so
    // we add all of its lobes "adjacently".

    // This function wraps the call to createLobes() ensuring that we
    // BsdfBuilder_endAdjacentComponents(bsdfBuilder) is called
    // before we return

    BsdfBuilder_startAdjacentComponents(bsdfBuilder);

    createLobes(me,
                bsdfBuilder,
                tls,
                state,
                uParams,
                vParams,
                label,
                resultCode);

    BsdfBuilder_endAdjacentComponents(bsdfBuilder);

    return;
}

