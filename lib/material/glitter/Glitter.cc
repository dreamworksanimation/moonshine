// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file Glitter.cc

#include "Glitter.h"

#include <moonray/map/primvar/Primvar.h>
#include <moonshine/common/interpolation/Interpolation.h>

#include <scene_rdl2/common/math/ColorSpace.h>
#include <scene_rdl2/common/math/MathUtil.h>
#include <scene_rdl2/render/util/Random.h>

#include <memory>

namespace moonshine {
namespace glitter {

using namespace moonray;

// Below 3 values are hardcoded - feel free to change for debugging / testing
#define MACROFLAKES_FULL_THRESHOLD 0.99f // If macroflakes cover 99% of footprint, no other LOD modes done
#define MACROFLAKES_MAX_THRESHOLD 0.9f   // If macroflakes cover > 90% of footprint, approximate micro flakes
#define MACROFLAKES_MIN_THRESHOLD 0.1f   // If macroflakes cover < 10% of footprint, don't do first LOD

#define GLITTERFLAKES_MIN_BLEND_START 100
#define GLITTERFLAKES_MAX_BLEND_START 900
#define GLITTERFLAKES_BLEND_START_RANGE (GLITTERFLAKES_MAX_BLEND_START - GLITTERFLAKES_MIN_BLEND_START)

namespace {
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
finline float
computeCoverageFactor(float flakeSize)
{
    // Clamp at high inflection point
    // See desmos graph: https://www.desmos.com/calculator/44kglnsfgy
    flakeSize = scene_rdl2::math::min(flakeSize, 1.1277f);

    float res = 0.0f;
    const float flakeSizeSq = flakeSize * flakeSize;
    if (flakeSize < 0.4f) {
        res = (-0.0382308f * flakeSize) + (1.230008f * flakeSizeSq);
    } else {
        res = 0.030076f - (0.6612578f * flakeSize) + (3.313868f * flakeSizeSq)
                - (1.7856762f * flakeSize * flakeSizeSq);
    }

    return scene_rdl2::math::saturate(res);
}

finline scene_rdl2::math::Color
applyColorVariationInHsv(const scene_rdl2::math::Color& baseColor,
                         const scene_rdl2::math::Vec3f& hsvVariation,
                         const scene_rdl2::math::Vec3f& randoms)
{
    float h = baseColor.r;
    float s = baseColor.g;
    float v = baseColor.b;

    // Hue variation is applied around the hue of the base flake color equally in clockwise and
    // anti-clockwise direction.
    // Saturation and Value variation is +/- variation clamped to [0, 1] range
    h += hsvVariation.x * randoms.x;
    h = h - scene_rdl2::math::floor(h); // Wrap Hue

    s += hsvVariation.y * randoms.y;
    s = scene_rdl2::math::saturate(s);

    v += hsvVariation.z * randoms.z;
    v = scene_rdl2::math::saturate(v);

    return scene_rdl2::math::Color(h, s, v);
}

} // anonymous namespace

// -----------------------------------------------------------------------------------------------
/// Member functions

Glitter::Glitter(const scene_rdl2::rdl2::Material* shader,
                 const ispc::GLITTER_UniformParameters& params)
{
    if (params.mSpace != ispc::SHADING_SPACE_OBJECT && params.mSpace != ispc::SHADING_SPACE_REFERENCE) {
        throw std::invalid_argument("Only object and reference space supported");
    }

    // Copy over uniform parameters
    mUniformParams.mSeed = params.mSeed;
    mUniformParams.mSpace = params.mSpace;
    mUniformParams.mDenseGlitterLodQuality = params.mDenseGlitterLodQuality;
    mUniformParams.mSearchRadiusFactor = params.mSearchRadiusFactor;
    mUniformParams.mLayeringMode = params.mLayeringMode;
    mUniformParams.mDebugMode = params.mDebugMode;

    // **Note** Flake randomness is capped at 0.6 because of the hack (to prevent black flakes) used in
    // GlitterGGXCookTorrance bsdf which replaces highly perturbed flake normals with the shading normal.
    // Consequently, for randomness values greater 0.6, the visual result starts appearing less
    // random since more and more flake normals start getting replaced with the shading normal
    mUniformParams.mFlakeRandomness = 0.6f * scene_rdl2::math::saturate(params.mFlakeRandomness);

    // Build a random number table for flake colors
    scene_rdl2::util::Random rng(params.mSeed + 100);
    for(int i = 0; i < ispc::NOISE_WORLEY_GLITTER_TABLE_SIZE; ++i) {
        mIspc.mRandomTable[i] = rng.getNextFloat();
    }

    mXform.reset();
    mIspc.mXform = nullptr;
    if (params.mSpace != ispc::SHADING_SPACE_REFERENCE) {
        // Construct Xform with default settings
        mXform = std::make_unique<moonray::shading::Xform>(shader, nullptr, nullptr, nullptr);
        mIspc.mXform = mXform->getIspcXform();
    }


    mNoiseWorley = std::make_unique<noise::Worley>((int)mUniformParams.mSeed,
                                                   ispc::NOISE_WORLEY_GLITTER_FLAKES_SIZE,
                                                   ispc::NOISE_WORLEY_V2,
                                                   ispc::NOISE_WORLEY_DIST_LINEAR,
                                                   mUniformParams.mFlakeRandomness,
                                                   true,   // add normals
                                                   true,   // use static table
                                                   false); // don't use map sort

    mIspc.mNoiseWorley = mNoiseWorley->getIspcWorley();

    for (unsigned int i = 0; i < ispc::NOISE_WORLEY_GLITTER_NUM_TEXTURES; ++i) {
        mFlakePatterns[i] = nullptr;
        mIspc.mFlakeTextureData[i] = nullptr;
        mIspc.mFlakeTextureCDF[i] = 0.0f;
    }
    // Map the LOD quality attribute to a range (mMicrofacetBlendStart, mMicrofacetBlendEnd). Start
    // determines at how many flakes in the footprint the microfacet approximation (3rd LOD mode) starts getting blended
    // with the microflake mode (2rd LOD).
    // The current mapping is:
    // Quality 0.0 - Start = 100 | quality 1.0 - start = 900
    // Blend range = start * 3 (so 300 for quality 0, 2700 for quality 1)
    // End = start + range -> clamped to NOISE_WORLY_MAX_SEARCH_POINTS (since we only store at most NOISE_WORLY_MAX_SEARCH_POINTS flakes).
    const float lodQuality = scene_rdl2::math::saturate(params.mDenseGlitterLodQuality);
    mIspc.mMicrofacetBlendStart = GLITTERFLAKES_MIN_BLEND_START + (lodQuality * GLITTERFLAKES_BLEND_START_RANGE);
    const int blendRange = 3.0f * mIspc.mMicrofacetBlendStart;
    mIspc.mMicrofacetBlendEnd = scene_rdl2::math::min(mIspc.mMicrofacetBlendStart + blendRange, ispc::NOISE_WORLEY_MAX_SEARCH_POINTS);

    // Assign refPKey to use in ispc
    mIspc.mRefPKey = moonray::shading::StandardAttributes::sRefP;
    mIspc.mRefNKey = moonray::shading::StandardAttributes::sRefN;
}

Glitter::Glitter(scene_rdl2::rdl2::Material* shader,
                 const std::vector<GlitterTexture>& glitterTextures,
                 scene_rdl2::rdl2::ShaderLogEventRegistry& logEventRegistry,
                 const ispc::GLITTER_UniformParameters& params):
                         Glitter(shader, params)
{
    // Update the Flake Texture
    scene_rdl2::rdl2::SceneObject* mapObj = static_cast<scene_rdl2::rdl2::SceneObject*>(shader);

    if (mapObj != nullptr) {
        scene_rdl2::rdl2::Map* map = mapObj->asA<scene_rdl2::rdl2::Map>();
        size_t count = 0;
        float prevWeight = 0.0f;
        for (auto const& t : glitterTextures) {
            mFlakePatterns[count] =
                std::make_unique<moonray::shading::BasicTexture>(map, logEventRegistry);
            std::string errorMsg;
            bool success = mFlakePatterns[count]->update(t.first,       // filename
                                                         ispc::TEXTURE_GAMMA_OFF,
                                                         moonray::shading::WrapType::Periodic, // wrapS
                                                         moonray::shading::WrapType::Periodic, // wrapT
                                                         false,         // use default color
                                                         scene_rdl2::math::sBlack,  // default color
                                                         scene_rdl2::math::sBlack,  // fatal color
                                                         errorMsg);

            if (success) {
                mIspc.mFlakeTextureData[count] = &mFlakePatterns[count]->getBasicTextureData();
                mIspc.mFlakeTextureCDF[count] = t.second + prevWeight;
                prevWeight += t.second;
            } else {
                mIspc.mFlakeTextureCDF[count] = prevWeight;
            }
            ++count;
        }
        if (prevWeight > 1.0f) {
            // Normalize CDF
            for (unsigned int i = 0; i < count; ++i) {
                mIspc.mFlakeTextureCDF[i] /= prevWeight;
            }
        }
    }
}

bool
Glitter::initializeNoiseSample(ispc::NOISE_WorleySample& noiseSample,
                               moonray::shading::TLState *tls,
                               const moonray::shading::State& state,
                               const ispc::GLITTER_VaryingParameters& params,
                               ispc::GLITTER_ResultCode& resultCode) const
{
    ispc::SHADING_Space returnSpace = mUniformParams.mSpace;

    ispc::PRIMVAR_Input_Source_Mode inputSourceMode;
    if (returnSpace == ispc::SHADING_SPACE_REFERENCE) {
        inputSourceMode = ispc::INPUT_SOURCE_MODE_REF_P_REF_N;
    } else {
        inputSourceMode = ispc::INPUT_SOURCE_MODE_P_N;
    }

    scene_rdl2::math::Vec3f pos_ddx, pos_ddy, pos_ddz, inputPosition;
    if (!primvar::getPosition(tls, state,
                              inputSourceMode,
                              inputPosition,
                              mXform.get(),
                              returnSpace,
                              mIspc.mRefPKey,
                              scene_rdl2::math::asCpp(noiseSample.position),
                              pos_ddx, pos_ddy, pos_ddz)) {
        resultCode = ispc::GLITTER_RESULTCODE_NO_REFP;
        return false;
    }

    scene_rdl2::math::Vec3f glitterNormal, inputNormal;
    if (!primvar::getNormal(tls, state,
                            inputSourceMode,
                            inputNormal,
                            mXform.get(),
                            returnSpace,
                            mIspc.mRefPKey,
                            mIspc.mRefNKey,
                            glitterNormal)) {
        resultCode = ispc::GLITTER_RESULTCODE_NO_REFN;
        return false;
    }

    // Ensure the Glitter Normal is Normalized
    scene_rdl2::math::asCpp(noiseSample.normal) = scene_rdl2::math::normalize(glitterNormal);

    scene_rdl2::math::Vec3f dPds, dPdt;
    if (returnSpace == ispc::SHADING_SPACE_REFERENCE) {
        state.getdVec3fAttrds(mIspc.mRefPKey, dPds);
        state.getdVec3fAttrdt(mIspc.mRefPKey, dPdt);
    } else {
        dPds = mXform->transformVector(ispc::SHADING_SPACE_RENDER,
                                       returnSpace,
                                       state,
                                       state.getdPds());
        dPdt = mXform->transformVector(ispc::SHADING_SPACE_RENDER,
                                       returnSpace,
                                       state,
                                       state.getdPdt());
    }

    // Multiplying the partials by frequency (density) 
    // can dramatically slow down performance.
    dPds *= params.mFlakeDensity;
    dPdt *= params.mFlakeDensity;
    scene_rdl2::math::Vec3f dPdxScaled = dPds * state.getdSdx() + dPdt * state.getdTdx();
    scene_rdl2::math::Vec3f dPdyScaled = dPds * state.getdSdy() + dPdt * state.getdTdy();

    scene_rdl2::math::asCpp(noiseSample.position) *= params.mFlakeDensity;
    scene_rdl2::math::asCpp(noiseSample.dNdx) = scene_rdl2::math::Vec3f(0, 0, 0);
    scene_rdl2::math::asCpp(noiseSample.dNdy) = scene_rdl2::math::Vec3f(0, 0, 0);
    noiseSample.footprintArea = scene_rdl2::math::length(cross(dPdxScaled, dPdyScaled));
    noiseSample.radius = scene_rdl2::math::sqrt(noiseSample.footprintArea * scene_rdl2::math::sOneOverPi);

    // mSearchRadiusFactor controls how much of the calculated radius is used
    // to search for worley points (0..1).   Larger values produce more stable
    // results at the cost of speed.   However, value of 0.25 is a good compromise
    // that is nearly indistinguishable from a value of 1.0.
    // This animation shows an animated wedge of mSearchRadiusFactor:
    // r_play /work/gshad/moonshine_test/glitterFlake/searchRadius/images/out_txt.*.exr
    noiseSample.searchRadius = noiseSample.radius * mUniformParams.mSearchRadiusFactor;

    noiseSample.footprintLength = 2.0f * noiseSample.radius;
    noiseSample.estimatedFeatures = (int)(ispc::NOISE_WORLEY_LAMBDA * noiseSample.footprintArea + 1);

    // Initialize blending factor between microflake and microfacet modes
    noiseSample.microfacetBlend = scene_rdl2::math::saturate((static_cast<float>(noiseSample.estimatedFeatures) - mIspc.mMicrofacetBlendStart) /
                                                 static_cast<float>(mIspc.mMicrofacetBlendEnd - mIspc.mMicrofacetBlendStart));
    
    noiseSample.compensateDeformation = calculateDeformationFactors(state, params.mCompensateDeformation, noiseSample, resultCode);
    return true;
}

bool
Glitter::calculateDeformationFactors(const moonray::shading::State& state,
                                     const bool compensateDeformation,
                                     ispc::NOISE_WorleySample& sample,
                                     ispc::GLITTER_ResultCode& resultCode) const
{
    if (!compensateDeformation || mUniformParams.mSpace != ispc::SHADING_SPACE_REFERENCE) {
        return false;
    }
    // Current Space Vectors
    const scene_rdl2::math::Vec3f curdPds = state.getdPds();
    const scene_rdl2::math::Vec3f curdPdt = state.getdPdt();

    scene_rdl2::math::Vec3f curZ = scene_rdl2::math::cross(curdPds, curdPdt);
    const float lengthCurZ = scene_rdl2::math::length(curZ);
    // Returns false if either curdPds or curdPdt are zero vectors or if they are identical.
    if (scene_rdl2::math::isZero(lengthCurZ)) {
        return false;
    }
    curZ = curZ / lengthCurZ;
    const scene_rdl2::math::Vec3f curX = scene_rdl2::math::normalize(curdPds);
    const scene_rdl2::math::Vec3f curY = scene_rdl2::math::normalize(scene_rdl2::math::cross(curZ, curX));

    // Reference Space Vectors
    scene_rdl2::math::Vec3f refdPds, refdPdt;
    state.getdVec3fAttrds(mIspc.mRefPKey, refdPds);
    state.getdVec3fAttrdt(mIspc.mRefPKey, refdPdt);

    scene_rdl2::math::Vec3f refZ = scene_rdl2::math::cross(refdPds, refdPdt);
    const float lengthRefZ = scene_rdl2::math::length(refZ);
    // Returns false if either refdPds or refdPdt are zero vectors or if they are identical.
    if (scene_rdl2::math::isZero(lengthRefZ)) {
        resultCode = ispc::GLITTER_RESULTCODE_NO_REFP_PARTIALS;
        return false;
    }
    refZ = refZ / lengthRefZ;
    const scene_rdl2::math::Vec3f refX = scene_rdl2::math::normalize(refdPds);
    const scene_rdl2::math::Vec3f refY = scene_rdl2::math::normalize(scene_rdl2::math::cross(refZ, refX));

    // Stretch/Compression Factors
    const float avgStretchC = scene_rdl2::math::abs(scene_rdl2::math::dot(curdPds, curX) / scene_rdl2::math::dot(refdPds, refX));
    const float avgStretchR = scene_rdl2::math::abs(scene_rdl2::math::dot(curdPdt, curY) / scene_rdl2::math::dot(refdPdt, refY));
    sample.compensationS = avgStretchC;
    sample.compensationT = avgStretchR;

    // Shear Factors along X Axis
    const float shearCurP_X = scene_rdl2::math::dot(scene_rdl2::math::normalize(curdPdt), curX);
    const float shearRefP_X = scene_rdl2::math::dot(scene_rdl2::math::normalize(refdPdt), refX);
    sample.shearRefP_X = shearRefP_X - shearCurP_X;

    // Reference Space Axes
    scene_rdl2::math::asCpp(sample.refX) = refX;
    scene_rdl2::math::asCpp(sample.refY) = refY;
    scene_rdl2::math::asCpp(sample.refZ) = refZ;

    return true;
}

unsigned int 
Glitter::finalizeFlakes(noise::Worley_PointArray::iterator flkItrBeg,
                        noise::Worley_PointArray::iterator flkItrCur)
{
    const unsigned int flakeCount = std::distance(flkItrBeg, flkItrCur);

    // The flake size is independent of flake density
    // so we need to sort all of the flakes even if the flake size is less than one.
    std::sort(flkItrBeg, flkItrCur,
            [](const ispc::NOISE_WorleyPoint f1, const ispc::NOISE_WorleyPoint f2)
            { return f1.id < f2.id; });
    return flakeCount;
}

unsigned int
Glitter::findNearestFlakes(const ispc::NOISE_WorleySample& sample,
                           const noise::Flake_StyleArray& styleCDF,
                           const noise::Flake_StyleArray& styleSizes,
                           const float flakeDensity,
                           const float flakeJitter,
                           noise::Worley_PointArray& flakes) const
{
    float flakeSize0 = styleSizes[0] * 0.5f;
    float flakeSize1 = styleSizes[1] * 0.5f;
    // Keeps the flake size constant regardless of density
    flakeSize0 *= flakeDensity;
    flakeSize1 *= flakeDensity;

    const noise::Flake_StyleArray searchRadius = {flakeSize0, flakeSize1};
    const noise::Flake_StyleArray* searchRadiusPtr = &searchRadius;
    const noise::RandomTable* randomTablePtr = &mIspc.mRandomTable;
    const noise::Flake_StyleArray* styleCDFPtr = &styleCDF;

    auto flkCurItr = mNoiseWorley->searchPoints(searchRadiusPtr,
                                                randomTablePtr,
                                                styleCDFPtr,
                                                sample,
                                                flakes,
                                                -1,
                                                flakeJitter);

    return finalizeFlakes(flakes.begin(), flkCurItr);
}

scene_rdl2::math::Color
Glitter::computeFlakeColor(const int flakeID,
                           const scene_rdl2::math::Color& baseColor,
                           const scene_rdl2::math::Vec3f& colorVariation) const
{
    // Early Exit
    if (scene_rdl2::math::isZero(colorVariation.x) && scene_rdl2::math::isZero(colorVariation.y) && scene_rdl2::math::isZero(colorVariation.z)) {
        return baseColor;
    }

    scene_rdl2::math::Vec3f randoms;
    const int tabMask = ispc::NOISE_WORLEY_GLITTER_TABLE_SIZE - 1;

    // Generate 3 random numbers, one each for variation in Hue, Saturation and Value
    randoms.x = mIspc.mRandomTable[(flakeID + 0) % tabMask] - 0.5f;          // [-0.5, 0.5]
    randoms.y = mIspc.mRandomTable[(flakeID + 1) % tabMask] * 2.0f - 1.0f;   // [-1, 1]
    randoms.z = mIspc.mRandomTable[(flakeID + 2) % tabMask] * 2.0f - 1.0f;   // [-1, 1]

    const scene_rdl2::math::Color hsv = scene_rdl2::math::rgbToHsv(baseColor);
    const scene_rdl2::math::Color modColor = applyColorVariationInHsv(hsv, colorVariation, randoms);
    return scene_rdl2::math::hsvToRgb(modColor);
}

// If the flake colors are varying, find the average color that represents an approximation
// by indexing into precomputed LUTs for average saturation and value which were generated
// by plotting ground truth average color for 16 base color hues (rows) and 100 hue variations (columns).
// See ispc/HueVariationTables.isph for more detail
// NOTE: Scaling for mCoverageFactor is baked into the LUT and otherwise accounted for by
// this function so there is no need to scale the result by it.
scene_rdl2::math::Color
Glitter::computeAverageFlakeColor(const scene_rdl2::math::Color& baseColor,
                                  const scene_rdl2::math::Vec3f& hsvVariation,
                                  const float coverageFactor) const
{
    if (scene_rdl2::math::isZero(hsvVariation.x) && scene_rdl2::math::isZero(hsvVariation.y) && scene_rdl2::math::isZero(hsvVariation.z)) {
        return baseColor;
    }

    const scene_rdl2::math::Color baseColorHsv = scene_rdl2::math::rgbToHsv(baseColor);
    const float hueVar = hsvVariation.x;
    const float origHue = baseColorHsv.r;
    const float origSat = baseColorHsv.g;
    const float origVal = baseColorHsv.b;

    // The plot of how the average color changes as the hue variation increases
    // is non-linear and varies depending on the original hue.  The way the plot
    // changes shows a recurring pattern that ping pongs three times across
    // the spectrum which is represented by the origHueT variable below.
    float origHueT = scene_rdl2::math::fmod(origHue, 0.3333f) / 0.3333f;
    origHueT = 2.0f * ((origHueT > 0.5f) ? 1.0f - origHueT : origHueT);

    const float hueIndexFloat = origHueT * 15.0f;
    const int hueIndexLo = static_cast<int>(hueIndexFloat);
    const int hueIndexHi = static_cast<int>(scene_rdl2::math::min(15.0f, scene_rdl2::math::floor(hueIndexFloat) + 1.0f));

    const float hueVarIndexFloat = hueVar * 99.0f;
    const int hueVarIndexLo = static_cast<int>(hueVarIndexFloat);
    const int hueVarIndexHi = static_cast<int>(scene_rdl2::math::min(99.0f, scene_rdl2::math::floor(hueVarIndexFloat) + 1.0f));

    const float hueT = hueIndexFloat - static_cast<float>(hueIndexLo);
    const float varT = hueVarIndexFloat - static_cast<float>(hueVarIndexLo);

    const float s0 = ispc::GLITTER_getHueVariationSaturation(hueIndexLo, hueVarIndexLo);
    const float s1 = ispc::GLITTER_getHueVariationSaturation(hueIndexLo, hueVarIndexHi);
    const float s2 = ispc::GLITTER_getHueVariationSaturation(hueIndexHi, hueVarIndexLo);
    const float s3 = ispc::GLITTER_getHueVariationSaturation(hueIndexHi, hueVarIndexHi);
    const float avgSat = origSat * scene_rdl2::math::lerp(scene_rdl2::math::lerp(s0, s1, varT), scene_rdl2::math::lerp(s2, s3, varT), hueT);

    const float v0 = ispc::GLITTER_getHueVariationValue(hueIndexLo, hueVarIndexLo);
    const float v1 = ispc::GLITTER_getHueVariationValue(hueIndexLo, hueVarIndexHi);
    const float v2 = ispc::GLITTER_getHueVariationValue(hueIndexHi, hueVarIndexLo);
    const float v3 = ispc::GLITTER_getHueVariationValue(hueIndexHi, hueVarIndexHi);
    float avgVal = origVal * scene_rdl2::math::lerp(scene_rdl2::math::lerp(v0, v1, varT), scene_rdl2::math::lerp(v2, v3, varT), hueT);
    avgVal = scene_rdl2::math::lerp(origVal * coverageFactor, avgVal, origSat);

    return scene_rdl2::math::hsvToRgb(scene_rdl2::math::Color(origHue, avgSat, avgVal));
}

scene_rdl2::math::Color
Glitter::computeAverageBaseColor(const ispc::GLITTER_VaryingParameters& params) const
{
    // Computes the average color of the flakes when there are multiple styles,
    // for LOD purposes.
    // Takes the size and frequency of each flake style into account
    // and lerps the colors in HSV space.
    if (scene_rdl2::math::isZero(params.mFlakeStyleFrequency[0])) {
        return scene_rdl2::math::asCpp(params.mFlakeColor[1]);
    }
    if (scene_rdl2::math::isZero(params.mFlakeStyleFrequency[1])) {
        return scene_rdl2::math::asCpp(params.mFlakeColor[0]);
    }
    const float size1Sqr = scene_rdl2::math::sqr(params.mFlakeSize[0]);
    const float size2Sqr = scene_rdl2::math::sqr(params.mFlakeSize[1]);
    const float size1Ratio = size1Sqr / (size1Sqr + size2Sqr);
    const float size2Ratio = size2Sqr / (size1Sqr + size2Sqr);
    const float flake1Proportion = size1Ratio * params.mFlakeStyleFrequency[0];
    const float flake2Proportion = size2Ratio * params.mFlakeStyleFrequency[1];
    const float t = flake2Proportion / (flake1Proportion + flake2Proportion);
    const scene_rdl2::math::Color color1 = scene_rdl2::math::asCpp(params.mFlakeColor[0]);
    const scene_rdl2::math::Color color2 = scene_rdl2::math::asCpp(params.mFlakeColor[1]);
    const scene_rdl2::math::Color flakeColor1HSV = scene_rdl2::math::rgbToHsv(color1);
    const scene_rdl2::math::Color flakeColor2HSV = scene_rdl2::math::rgbToHsv(color2);
    const scene_rdl2::math::Color averageFlakeColorHSV = interpolation::lerpHSV(t, flakeColor1HSV, flakeColor2HSV);
    return scene_rdl2::math::hsvToRgb(averageFlakeColorHSV);
}

int
Glitter::chooseFlakePattern(int id) const
{
    const int tabMask = ispc::NOISE_WORLEY_GLITTER_TABLE_SIZE - 1;
    // Choose a Random Number
    const float r1 = mIspc.mRandomTable[(7*id) & tabMask];      // [0, 1]
    if (r1 <= mIspc.mFlakeTextureCDF[0]) {
        return 0;
    } else if (r1 <= mIspc.mFlakeTextureCDF[1]) {
        return 1;
    }
    // Regular Circular Flakes
    return -1;
}

void
Glitter::rotateFlakeUVs(int id,
                        const float flakeOrientationRandomness,
                        scene_rdl2::math::Vec2f& uv) const
{
    const int tabMask = ispc::NOISE_WORLEY_GLITTER_TABLE_SIZE - 1;
    // Choose a Random Number
    float r1 = mIspc.mRandomTable[(29*id) & tabMask];      // [0, 1]
    // [-0.5, 0.5]
    r1 -= 0.5f;
    const float theta = r1 * scene_rdl2::math::sTwoPi * flakeOrientationRandomness;
    float sinTheta, cosTheta;
    scene_rdl2::math::sincos(theta, &sinTheta, &cosTheta);
    const float newU = uv[0] * cosTheta - uv[1] * sinTheta;
    const float newV = uv[0] * sinTheta + uv[1] * cosTheta;
    uv[0] = newU;
    uv[1] = newV;
}

void
Glitter::readFlakeTexturesAndModifyWeights(moonray::shading::TLState* tls,
                                           const moonray::shading::State& state,
                                           const unsigned int macroFlakeCount,
                                           const float flakeOrientationRandomness,
                                           noise::Worley_PointArray& flakes,
                                           std::array<scene_rdl2::math::Color, sMaxMacroFlakeCount>& flakeTextures) const
{
    float derivatives[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    for (unsigned int i = 0; i < macroFlakeCount; ++i) {
        // default white
        flakeTextures[i] = scene_rdl2::math::sWhite;

        int patternIndex = chooseFlakePattern(flakes[i].id);
        // Do we have a valid index
        if (patternIndex >= 0 && mFlakePatterns[patternIndex] != nullptr && mFlakePatterns[patternIndex]->isValid()) {
            scene_rdl2::math::Vec2f uv = scene_rdl2::math::asCpp(flakes[i].uv);
            if (!scene_rdl2::math::isZero(flakeOrientationRandomness)) {
                rotateFlakeUVs(flakes[i].id,
                               flakeOrientationRandomness,
                               uv);
            }
            // translate from [-1.0, 1.0] to [0.0, 1.0]
            uv[0] = (uv[0] + 1.0f) * 0.5f;
            uv[1] = (uv[1] + 1.0f) * 0.5f;
            flakeTextures[i] = mFlakePatterns[patternIndex]->sample(tls,
                                                                    state,
                                                                    uv,
                                                                    derivatives);
            flakes[i].weight *= scene_rdl2::math::luminance(flakeTextures[i]);
        }
    }
}

void
Glitter::computeStyleCDF(const noise::Flake_StyleArray& styleFrequencies,
                         noise::Flake_StyleArray& styleCDF) const
{
    float styleWeight = 0.0f;
    for (size_t i = 0; i < ispc::NOISE_WORLEY_GLITTER_NUM_STYLES; i++) {
        styleCDF[i] = styleFrequencies[i] + styleWeight;
        styleWeight += styleFrequencies[i];
    }
    if (styleWeight > 1.0f) {
        for (size_t i = 0; i < ispc::NOISE_WORLEY_GLITTER_NUM_STYLES; i++) {
            styleCDF[i] /= styleWeight;
        }
    }
}

void
Glitter::setFlakeStyles(const ispc::GLITTER_VaryingParameters& params,
                        const unsigned int macroFlakeCount,
                        const noise::Worley_PointArray& flakes,
                        std::array<scene_rdl2::math::Color, sMaxMacroFlakeCount>& flakeColors,
                        std::array<float, sMaxMacroFlakeCount>& flakeRoughnesses) const
{
    for (unsigned int i = 0; i < macroFlakeCount; ++i) {
        int styleIndex = flakes[i].styleIndex;
        // Do we have a valid index
        if (styleIndex >= 0) {
            flakeColors[i] = scene_rdl2::math::asCpp(params.mFlakeColor[styleIndex]);
            flakeRoughnesses[i] = params.mFlakeRoughness[styleIndex];
        } else {
            flakeColors[i] = scene_rdl2::math::sBlack;
            flakeRoughnesses[i] = 0.0f;
        }
    }
}

// Add up to 4 macro flakes.
// If only 1 flake covers entire pixel, set singleMacroFlake to true
scene_rdl2::math::Color
Glitter::createMacroFlakes(moonray::shading::BsdfBuilder &bsdfBuilder,
                           const unsigned int macroFlakeCount,
                           const unsigned int totalFlakeCount,
                           const float macroFlakeVisibility,
                           const scene_rdl2::math::ReferenceFrame& refFrame,
                           const noise::Worley_PointArray& flakes,
                           const std::array<scene_rdl2::math::Color, sMaxMacroFlakeCount>& baseColor,
                           const std::array<float, sMaxMacroFlakeCount>& flakeRoughnesses,
                           const std::array<scene_rdl2::math::Color, sMaxMacroFlakeCount>& flakeTextures,
                           const scene_rdl2::math::Vec3f& colorVariation,
                           const float mask,
                           const int label,
                           const float coverageFactor,
                           const ispc::BsdfBuilderBehavior combineBehavior,
                           bool& singleMacroFlake) const
{
    // assume we'll only create one for now
    singleMacroFlake = true;

    scene_rdl2::math::Color accumMacroFlakesColor = scene_rdl2::math::sBlack;

    // Check the flakes to see if there is one with coverage of nearly 1
    noise::Worley_PointArray::const_iterator itr = std::find_if(flakes.begin(), flakes.begin()+macroFlakeCount,
                                                  [](const ispc::NOISE_WorleyPoint flake)
                                                  { return flake.weight >= scene_rdl2::math::sOneMinusEpsilon; });

    // If we are close enough to the surface that there's only one glitter flake in the ray
    // footprint, or, if more flakes are present with one flake completely covering the ray footprint,
    // add only one ggx lobe representing this flake for the shading point.
    scene_rdl2::math::Color currFlakeColor = scene_rdl2::math::sBlack;
    scene_rdl2::math::Vec3f currNormal;
    float currFlakeRoughness;
    float flakeWeight = 1.0f;
    if (macroFlakeCount == 1) {
        scene_rdl2::math::Color flakeColor = baseColor[0] * flakeTextures[0];
        currFlakeColor = computeFlakeColor(flakes[0].id, flakeColor, colorVariation);
        currNormal = refFrame.localToGlobal(scene_rdl2::math::asCpp(flakes[0].normal));
        currFlakeRoughness = flakeRoughnesses[0];
        flakeWeight = flakes[0].weight;
    } else if (std::distance(flakes.begin(), itr) < (int)macroFlakeCount) { // found flake with > 1 coverage
        int flakeID = std::distance(flakes.begin(), itr);
        scene_rdl2::math::Color flakeColor = baseColor[flakeID] * flakeTextures[flakeID];
        currFlakeColor = computeFlakeColor((*itr).id, flakeColor, colorVariation);
        currNormal = refFrame.localToGlobal(scene_rdl2::math::asCpp((*itr).normal));
        currFlakeRoughness = flakeRoughnesses[flakeID];
    }

    // If either of the two conditions above are true (1 flake with full pixel coverage), then currFlakeColor
    // is initialized to a non-black flake color - add one ggx lobe for that flake and we're done shading
    if (!isBlack(currFlakeColor)) {
        moonray::shading::GlitterFlakeBRDF glitterFlakeRefl(refFrame.getN(),
                                                   currNormal,
                                                   currFlakeColor,
                                                   currFlakeColor,
                                                   currFlakeRoughness);

        bsdfBuilder.addGlitterFlakeBRDF(glitterFlakeRefl,
                                        mask * flakeWeight,
                                        combineBehavior,
                                        label);

        accumMacroFlakesColor = currFlakeColor;
        return accumMacroFlakesColor;
    }

    const float oneOverMacroFlakeVisibility = scene_rdl2::math::rcp(macroFlakeVisibility);

    // more than one flake contributes to the shading point's result
    singleMacroFlake = false;

    for (unsigned int i = 0; i < macroFlakeCount; ++i) {
        scene_rdl2::math::Color flakeColor = baseColor[i] * flakeTextures[i];
        currFlakeColor = computeFlakeColor(flakes[i].id, flakeColor, colorVariation);
        currNormal = refFrame.localToGlobal(scene_rdl2::math::asCpp(flakes[i].normal));
        currFlakeRoughness = flakeRoughnesses[i];
        float scale = flakes[i].weight;

        // If there are more flakes to add after macroflakes, we're far enough away to
        // not see gaps between flakes - scale by coverage factor
        scale *= (totalFlakeCount > macroFlakeCount) ? coverageFactor : 1.0f;

        // If total visibility of the macro flakes is greater than 1.0, normalize their contribution
        if (macroFlakeVisibility >= scene_rdl2::math::sOneMinusEpsilon) scale *= oneOverMacroFlakeVisibility;

        // Each lobe is scaled by the "average coverage per unit area" in order to account
        // for potential overlap between the flakes
        if (!scene_rdl2::math::isBlack(currFlakeColor)) {
            moonray::shading::GlitterFlakeBRDF glitterFlakeRefl(refFrame.getN(),
                                                       currNormal,
                                                       currFlakeColor,
                                                       currFlakeColor,
                                                       currFlakeRoughness);

            bsdfBuilder.addGlitterFlakeBRDF(glitterFlakeRefl,
                                            scale * mask,
                                            combineBehavior,
                                            label);

            accumMacroFlakesColor += currFlakeColor;
        }
    }

    return accumMacroFlakesColor;
}

scene_rdl2::math::Color
Glitter::createMicroFlake(const ispc::GLITTER_VaryingParameters& params,
                          moonray::shading::BsdfBuilder &bsdfBuilder,
                          const unsigned int microFlakeCount,
                          const unsigned int macroFlakeCount,
                          scene_rdl2::alloc::Arena* arena,
                          const noise::Worley_PointArray& flakes,
                          const scene_rdl2::math::ReferenceFrame& refFrame,
                          const float microFlakeVis,
                          const std::array<scene_rdl2::math::Color, sMaxMacroFlakeCount>& baseColor,
                          const scene_rdl2::math::Vec3f& colorVariation,
                          const float mask,
                          const int label,
                          const float coverageFactor,
                          const ispc::BsdfBuilderBehavior combineBehavior) const
{
    scene_rdl2::math::Color avgColor = scene_rdl2::math::sBlack;

    const unsigned int offset = macroFlakeCount;

    scene_rdl2::math::Vec3f* flakeNormals = arena->allocArray<scene_rdl2::math::Vec3f>(microFlakeCount);
    scene_rdl2::math::Color* flakeColors = arena->allocArray<scene_rdl2::math::Color>(microFlakeCount);

    // Add a microflake lobe
    if (microFlakeVis > 0.0f) {
        // Compute average color for use in both microflake and microfacet lobes
        // (populating the individual flake colors on the way)
        // Also populate the normals to pass to the StochasticFlakes Bsdf lobe
        for (unsigned int i = 0; i < microFlakeCount; ++i) {
            // modulus the (i+offset) index by macroFlakeCount to pick from the flake colors chosen for macroFlakes.
            const int flakeIndex = i + offset;
            const int limitedFlakeIndex = flakeIndex % macroFlakeCount;
            flakeColors[i] = computeFlakeColor(flakes[flakeIndex].id, baseColor[limitedFlakeIndex], colorVariation);
            avgColor += flakeColors[i];
            flakeNormals[i] = refFrame.localToGlobal(scene_rdl2::math::asCpp(flakes[flakeIndex].normal));
        }

        // TODO: The StochasticFlakes lobe can be passed an array of roughnesses
        // rather than just picking the first one.
        moonray::shading::StochasticFlakesBRDF stochasticFlakeRefl(refFrame.getN(),
                                                          flakeNormals,
                                                          flakeColors,
                                                          microFlakeCount,
                                                          params.mFlakeRoughness[0],
                                                          mUniformParams.mFlakeRandomness);

        bsdfBuilder.addStochasticFlakesBRDF(stochasticFlakeRefl,
                                            microFlakeVis * coverageFactor * mask,
                                            combineBehavior,
                                            label);
    }

    return avgColor * scene_rdl2::math::rcp((float)microFlakeCount);
}

// The debug mode helps visualize the contributions from each of the
// three modes that blend together.
// Red reprenents macro flake contribution
// Green represents micro flake contribution
// Blue represents ggx microfacet contribution
// The three always add up to 1.0 unless insufficient number
// of flakes are found in the ray footprint.
void
Glitter::createDebugBlendLobes(moonray::shading::TLState *tls,
                               const moonray::shading::State& state,
                               moonray::shading::BsdfBuilder& bsdfBuilder,
                               const ispc::GLITTER_VaryingParameters& params,
                               ispc::GLITTER_ResultCode& resultCode) const
{
    static const scene_rdl2::math::Color sMacroFlakeDebugColor(1.0f, 0.0f, 0.0f);
    static const scene_rdl2::math::Color sMicroFlakeDebugColor(0.0f, 1.0f, 0.0f);
    static const scene_rdl2::math::Color sMicroFacetDebugColor(0.0f, 0.0f, 1.0f);

    if (!state.isHifi()) {
        bsdfBuilder.addEmission(sMicroFacetDebugColor);
        return;
    }

    ispc::NOISE_WorleySample sample;
    const bool isValidNoiseSample = initializeNoiseSample(sample, tls, state, params, resultCode);
    // Early exit, likely because no refN was provided.
    if (!isValidNoiseSample) {
        return;
    }

    if (scene_rdl2::math::isOne(sample.microfacetBlend)) {
        bsdfBuilder.addEmission(sMicroFacetDebugColor);
        return;
    }

    noise::Flake_StyleArray styleCDF;
    computeStyleCDF(params.mFlakeStyleFrequency, styleCDF);

    noise::Worley_PointArray flakes;
    const unsigned int flakeCount = findNearestFlakes(sample,
                                                      styleCDF,
                                                      params.mFlakeSize,
                                                      params.mFlakeDensity,
                                                      params.mFlakeJitter,
                                                      flakes);

    // Early exit
    if (flakeCount == 0) {
        return;
    }

    unsigned int macroFlakeCount = scene_rdl2::math::min(flakeCount, sMaxMacroFlakeCount);

    // Gather the total visibility of the first 4 flakes
    float macroFlakesVis = 0.0f;
    for (unsigned int i = 0; i < macroFlakeCount; ++i) {
        macroFlakesVis += flakes[i].weight;
    }

    if (!(macroFlakesVis > MACROFLAKES_MIN_THRESHOLD)) {
        macroFlakesVis = 0.0f;
    }

    const unsigned int microFlakeCount = flakeCount - macroFlakeCount;

    const float microFlakeVis = (microFlakeCount > 0) ? (1.0f - sample.microfacetBlend) * (1.0f - macroFlakesVis) : 0.f;
    const float microFacetVis = (microFlakeCount > 0) ? sample.microfacetBlend * (1.0f - macroFlakesVis) : 0.f;

    bsdfBuilder.addEmission(scene_rdl2::math::clamp(macroFlakesVis) * sMacroFlakeDebugColor +
                            scene_rdl2::math::clamp(microFlakeVis)  * sMicroFlakeDebugColor +
                            scene_rdl2::math::clamp(microFacetVis)  * sMicroFacetDebugColor);
}

void
Glitter::createDebugColorLobes(moonray::shading::TLState *tls,
                               const moonray::shading::State& state,
                               moonray::shading::BsdfBuilder& bsdfBuilder,
                               const ispc::GLITTER_VaryingParameters& params,
                               ispc::GLITTER_ResultCode& resultCode) const
{
    const scene_rdl2::math::Vec3f colorVariation = scene_rdl2::math::asCpp(params.mFlakeHSVColorVariation);
    scene_rdl2::math::Color emissionColor = scene_rdl2::math::sBlack;
    scene_rdl2::math::Color macroFlakesAvgColor = scene_rdl2::math::sBlack;
    scene_rdl2::math::Color currFlakeColor = scene_rdl2::math::sBlack;

    float coverageFactor = mIspc.mCoverageFactor;
    float flakeSize = params.mFlakeSize[0] * params.mFlakeStyleFrequency[0] +
                      params.mFlakeSize[1] * params.mFlakeStyleFrequency[1];
    flakeSize *= params.mFlakeDensity;
    coverageFactor = computeCoverageFactor(flakeSize);

    ispc::NOISE_WorleySample sample;
    const bool isValidNoiseSample = initializeNoiseSample(sample, tls, state, params, resultCode);
    // Early exit, likely because no refN was provided.
    if (!isValidNoiseSample) {
        return;
    }

    if (scene_rdl2::math::isOne(sample.microfacetBlend)) {
        scene_rdl2::math::Color averageFlakeColor = computeAverageBaseColor(params);
        averageFlakeColor = computeAverageFlakeColor(averageFlakeColor, colorVariation, coverageFactor);
        emissionColor = averageFlakeColor;
        bsdfBuilder.addEmission(emissionColor);
        return;
    }

    noise::Flake_StyleArray styleCDF;
    computeStyleCDF(params.mFlakeStyleFrequency, styleCDF);

    noise::Worley_PointArray flakes;
    const unsigned int flakeCount = findNearestFlakes(sample,
                                                      styleCDF,
                                                      params.mFlakeSize,
                                                      params.mFlakeDensity,
                                                      params.mFlakeJitter,
                                                      flakes);

    if (flakeCount == 0) {
        return;
    }

    const unsigned int macroFlakeCount = scene_rdl2::math::min(flakeCount, sMaxMacroFlakeCount);

    std::array<scene_rdl2::math::Color, sMaxMacroFlakeCount> baseColor;
    std::array<float, sMaxMacroFlakeCount> flakeRoughnesses;
    setFlakeStyles(params,
                   macroFlakeCount,
                   flakes,
                   baseColor,
                   flakeRoughnesses);

    // Gather the total visibility of the first 4 flakes
    float macroFlakeVis = 0.0f;
    for (unsigned int i = 0; i < macroFlakeCount; ++i) {
        macroFlakeVis += flakes[i].weight;
    }

    noise::Worley_PointArray::iterator itr = std::find_if(flakes.begin(), flakes.begin()+macroFlakeCount,
                                                  [](const ispc::NOISE_WorleyPoint flake)
                                                  { return flake.weight >= scene_rdl2::math::sOneMinusEpsilon; });

    // Single macro flake
    float flakeWeight = 1.0f;
    if (macroFlakeCount == 1) {
        currFlakeColor = computeFlakeColor(flakes[0].id, baseColor[0], colorVariation);
        flakeWeight = flakes[0].weight;
    } else if (std::distance(flakes.begin(), itr) < (int)macroFlakeCount) { // found flake with > 1 coverage
        const int flakeID = std::distance(flakes.begin(), itr);
        currFlakeColor = computeFlakeColor((*itr).id, baseColor[flakeID], colorVariation);
        flakeWeight = (*itr).weight;
    }

    // If either of the two conditions above are true (1 flake with full pixel coverage), then currFlakeColor
    // is initialized to a non-black flake color - set emission color once for that flake and we're done shading
    if (!scene_rdl2::math::isBlack(currFlakeColor)) {
        emissionColor = currFlakeColor * flakeWeight;
        bsdfBuilder.addEmission(emissionColor);
        return;
    }

    // Macro flakes
    const float oneOverMacroFlakeVisibility = scene_rdl2::math::rcp(macroFlakeVis);
    for (unsigned int i = 0; i < macroFlakeCount; ++i) {
        currFlakeColor = computeFlakeColor(flakes[i].id, baseColor[i], colorVariation);
        float scale = flakes[i].weight;
        scale *= (flakeCount > macroFlakeCount) ? coverageFactor : 1.0f;

        // If total visibility of the macro flakes is greater than 1.0, normalize their contribution
        if (macroFlakeVis >= scene_rdl2::math::sOneMinusEpsilon) scale *= oneOverMacroFlakeVisibility;

        // Each lobe is scaled by the "average coverage per unit area" in order to account
        // for potential overlap between the flakes
        if (!scene_rdl2::math::isBlack(currFlakeColor)) {
            emissionColor += currFlakeColor * scale;
            macroFlakesAvgColor += currFlakeColor;
        }
    }

    // Consider all remaining flakes for the microflake model
    const unsigned int microFlakeCount = flakeCount - macroFlakeCount;

    // Only macro flakes
    if (macroFlakeVis < 1.0f && microFlakeCount == 0) {
        emissionColor = scene_rdl2::math::Color(scene_rdl2::math::saturate(emissionColor.r), 
                                    scene_rdl2::math::saturate(emissionColor.g), 
                                    scene_rdl2::math::saturate(emissionColor.b));
        bsdfBuilder.addEmission(emissionColor);
        return;
    }

    const float microFlakeVis = scene_rdl2::math::saturate((1.0f - sample.microfacetBlend) * (1.0f - macroFlakeVis));
    const float microFacetVis = scene_rdl2::math::saturate(sample.microfacetBlend * (1.0f - macroFlakeVis));

    if (macroFlakeVis >= MACROFLAKES_MAX_THRESHOLD && microFlakeCount > 0) {
        // Macro flakes > 90%
        macroFlakesAvgColor /= static_cast<float>(macroFlakeCount);
        if (macroFlakeVis < MACROFLAKES_FULL_THRESHOLD && !isBlack(macroFlakesAvgColor)) {
            // Macro flakes < 99%
            float oneMinusMacroFlakesVis = 1.0f - macroFlakeVis;
            emissionColor += macroFlakesAvgColor * oneMinusMacroFlakesVis * coverageFactor;
        }
    } else {
        // Macro fales < 90% so do micro flakes
        scene_rdl2::math::Color microFlakesAvgColor = scene_rdl2::math::sBlack;
        if (microFlakeVis > 0.0f) {
            // microflakes
            const unsigned int offset = macroFlakeCount;
            for (unsigned int i = 0; i < microFlakeCount; ++i) {
                // modulus the (i+offset) index by macroFlakeCount to pick from the flake colors chosen for macroFlakes.
                const int flakeIndex = i + offset;
                const int limitedFlakeIndex = flakeIndex % macroFlakeCount;
                microFlakesAvgColor += computeFlakeColor(
                    flakes[flakeIndex].id, baseColor[limitedFlakeIndex], colorVariation);
            }
            microFlakesAvgColor /= static_cast<float>(microFlakeCount);
            emissionColor += microFlakesAvgColor * microFlakeVis * coverageFactor;
        }

        // Micro facet
        if (microFacetVis > 0.0f && !isBlack(microFlakesAvgColor)) {
            // microfacet
            emissionColor += microFlakesAvgColor * microFacetVis * coverageFactor;
        }
    }

    emissionColor = scene_rdl2::math::Color(
        scene_rdl2::math::saturate(emissionColor.r), 
        scene_rdl2::math::saturate(emissionColor.g), 
        scene_rdl2::math::saturate(emissionColor.b)
    );

    bsdfBuilder.addEmission(emissionColor);
}

void
Glitter::createDebugAverageColorLobes(moonray::shading::TLState *tls,
                                      const moonray::shading::State& state,
                                      moonray::shading::BsdfBuilder& bsdfBuilder,
                                      const ispc::GLITTER_VaryingParameters& params) const
{
    const scene_rdl2::math::Vec3f colorVariation = scene_rdl2::math::asCpp(params.mFlakeHSVColorVariation);
    scene_rdl2::math::Color averageColor = computeAverageBaseColor(params);

    float coverageFactor = 1.0f;
    float flakeSize = (params.mFlakeSize[0] * params.mFlakeStyleFrequency[0] +
                       params.mFlakeSize[1] * params.mFlakeStyleFrequency[1]) *
                       params.mFlakeDensity;
    coverageFactor = computeCoverageFactor(flakeSize);

    averageColor = computeAverageFlakeColor(averageColor, colorVariation, coverageFactor);

    bsdfBuilder.addEmission(averageColor);
}

void
Glitter::createDebugSampleLobes(moonray::shading::TLState *tls,
                                const moonray::shading::State& state,
                                moonray::shading::BsdfBuilder& bsdfBuilder,
                                const ispc::GLITTER_VaryingParameters& params,
                                const int mode,
                                ispc::GLITTER_ResultCode& resultCode) const
{
    ispc::NOISE_WorleySample sample;
    const bool isValidNoiseSample = initializeNoiseSample(sample, tls, state, params, resultCode);
    // Early exit, likely because no refN was provided.
    if (!isValidNoiseSample) {
        return;
    }

    if (mode == ispc::GLITTER_DEBUG_MODE_FOOTPRINT_AREA) {
        bsdfBuilder.addEmission(scene_rdl2::math::Color(sample.footprintArea));
    } else  if (mode == ispc::GLITTER_DEBUG_MODE_RADIUS) {
        bsdfBuilder.addEmission(scene_rdl2::math::Color(sample.radius));
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
// 500 and NOISE_WORLY_MAX_SEARCH_POINTS flakes - and exclusively to the single ggx lobe for >NOISE_WORLY_MAX_SEARCH_POINTS).
// The idea behind this is that when such large number of flakes are found, the shading point is usually at a far enough
// distance from the camera that individual flake contributions are not visually perceptible.
void
Glitter::createLobes(moonray::shading::TLState* tls,
                     const moonray::shading::State& state,
                     moonray::shading::BsdfBuilder& bsdfBuilder,
                     const ispc::GLITTER_VaryingParameters& params,
                     const int label,
                     ispc::GLITTER_ResultCode& resultCode) const
{
    if (mUniformParams.mDebugMode != ispc::GLITTER_DEBUG_MODE_OFF) {
        switch(mUniformParams.mDebugMode) {
        case ispc::GLITTER_DEBUG_MODE_BLEND:
            createDebugBlendLobes(tls,
                                  state,
                                  bsdfBuilder,
                                  params,
                                  resultCode);
            break;
        case ispc::GLITTER_DEBUG_MODE_COLOR:
            createDebugColorLobes(tls,
                                  state,
                                  bsdfBuilder,
                                  params,
                                  resultCode);
            break;
        case ispc::GLITTER_DEBUG_MODE_AVERAGE_COLOR:
            createDebugAverageColorLobes(tls,
                                         state,
                                         bsdfBuilder,
                                         params);
            break;
        default:
            createDebugSampleLobes(tls,
                                   state,
                                   bsdfBuilder,
                                   params,
                                   mUniformParams.mDebugMode,
                                   resultCode);
        }
        return;
    }

    // The noise sample contains all spatial information for sampling in noise space
    ispc::NOISE_WorleySample sample;
    const bool isValidNoiseSample = initializeNoiseSample(sample, tls, state, params, resultCode);
    // Early exit, likely because no refN was provided.
    if (!isValidNoiseSample) {
        return;
    }

    const float glitterMask = params.mGlitterMask;

    // Computes the tangent frame based on reference space
    scene_rdl2::math::ReferenceFrame refFrame(state.adaptNormal(state.getN()));

    const scene_rdl2::math::Vec3f hsvColorVariation = scene_rdl2::math::asCpp(params.mFlakeHSVColorVariation);

    // Early exit before doing anything with glitter
    if (scene_rdl2::math::isZero(glitterMask)) {
        return;
    }

    float coverageFactor = mIspc.mCoverageFactor;
    float flakeSize = params.mFlakeSize[0] * params.mFlakeStyleFrequency[0] +
                      params.mFlakeSize[1] * params.mFlakeStyleFrequency[1];
    flakeSize *= params.mFlakeDensity;
    coverageFactor = computeCoverageFactor(flakeSize);

    // Avoid gathering flakes for non-primary rays or if we know that we're far enough away
    // for only microfacet blending to contribute to this sample
    if ((!state.isHifi() && params.mApproximateForSecRays) || scene_rdl2::math::isOne(sample.microfacetBlend)) {

        scene_rdl2::math::Color averageFlakeColor = computeAverageBaseColor(params);
        averageFlakeColor = computeAverageFlakeColor(averageFlakeColor, hsvColorVariation, coverageFactor);

        moonray::shading::MicrofacetIsotropicBRDF microfacetRefl(refFrame.getN(),
                                                        averageFlakeColor,
                                                        averageFlakeColor,
                                                        mUniformParams.mFlakeRandomness,
                                                        ispc::MICROFACET_DISTRIBUTION_GGX,
                                                        ispc::MICROFACET_GEOMETRIC_SMITH);

        bsdfBuilder.addMicrofacetIsotropicBRDF(microfacetRefl,
                                               coverageFactor * glitterMask,
                                               ispc::BSDFBUILDER_PHYSICAL,
                                               label);

        return;
    }

    // At this point, we know gathering flakes around the shading point is necessary
    // (primary rays with ray footprint small enough for at least microflake computation)
    bsdfBuilder.startAdjacentComponents();

    // For primary and mirror rays use the user specified layering mode, otherwise
    // force the layering mode to be PHYSICAL for secondary and non-mirror rays
    // to prevent potentially significant energy gain with multiple bounces,
    // especially due to our use of a cook-torrance reflection lobe to approximate
    // dense glitter for secondary rays
    const ispc::GLITTER_LayeringModes layeringMode = state.isHifi() ?
        static_cast<ispc::GLITTER_LayeringModes>(mUniformParams.mLayeringMode) :
        ispc::GLITTER_LAYERING_MODE_PHYSICAL;

    const bool isAdditive = (layeringMode == ispc::GLITTER_LAYERING_MODE_ADDITIVE);

    const ispc::BsdfBuilderBehavior combineBehavior = isAdditive ?
                                                      ispc::BSDFBUILDER_ADDITIVE :
                                                      ispc::BSDFBUILDER_PHYSICAL;

    noise::Flake_StyleArray styleCDF;
    computeStyleCDF(params.mFlakeStyleFrequency, styleCDF);

    // Gather flakes around the shading point, with the nearest 10 flakes ordered by ID
    noise::Worley_PointArray flakes;
    const unsigned int flakeCount = findNearestFlakes(sample,
                                                      styleCDF,
                                                      params.mFlakeSize,
                                                      params.mFlakeDensity,
                                                      params.mFlakeJitter,
                                                      flakes);

    // Early exit after gathering flakes - no glitter lobes added
    if (flakeCount == 0) {
        bsdfBuilder.endAdjacentComponents();
        return;
    }

    unsigned int macroFlakeCount = scene_rdl2::math::min(flakeCount, sMaxMacroFlakeCount);

    std::array<scene_rdl2::math::Color, sMaxMacroFlakeCount> baseFlakeColors;
    std::array<float, sMaxMacroFlakeCount> flakeRoughnesses;
    setFlakeStyles(params,
                   macroFlakeCount,
                   flakes,
                   baseFlakeColors,
                   flakeRoughnesses);

    std::array<scene_rdl2::math::Color, sMaxMacroFlakeCount> flakeTextures;
    readFlakeTexturesAndModifyWeights(tls, state,
                                      macroFlakeCount,
                                      params.mFlakeOrientationRandomness,
                                      flakes,
                                      flakeTextures);

    // Gather the total visibility of the first 4 flakes
    float macroFlakesVis = 0.0f;
    for (unsigned int i = 0; i < macroFlakeCount; ++i) {
        macroFlakesVis += flakes[i].weight;
    }

    scene_rdl2::math::Color macroFlakesAvgColor = scene_rdl2::math::sBlack;
    bool createdSingleMacroFlake = false;

    // Create GGX lobes if the total visibility is greater than ggxMinThreshold (default 10%)
    if (macroFlakesVis > MACROFLAKES_MIN_THRESHOLD) {
        macroFlakesAvgColor = createMacroFlakes(bsdfBuilder, macroFlakeCount, flakeCount, macroFlakesVis,
                                                refFrame, flakes,
                                                baseFlakeColors,
                                                flakeRoughnesses,
                                                flakeTextures,
                                                hsvColorVariation,
                                                glitterMask,
                                                label,
                                                coverageFactor,
                                                combineBehavior,
                                                createdSingleMacroFlake);
    }

    if (createdSingleMacroFlake) {
        bsdfBuilder.endAdjacentComponents();
        return;
    }

    // Exit early if there are more than one macro flake returned and their coverage
    // is almost one.
    if (macroFlakesVis >= scene_rdl2::math::sOneMinusEpsilon) {
        bsdfBuilder.endAdjacentComponents();
        return;
    }

    // Consider all remaining flakes for the microflake model
    const unsigned int microFlakeCount = flakeCount - macroFlakeCount;

    // If there are only enough flakes for macro flakes (no more flakes left in the footprint for
    // microflakes) and they together do not cover the entire footprint, shade the under material
    // for the remaining coverage
    if (macroFlakesVis < 1.0f && microFlakeCount == 0) {
        bsdfBuilder.endAdjacentComponents();
        return;
    }

    const float microFlakeVis = scene_rdl2::math::saturate((1.0f - sample.microfacetBlend) * (1.0f - macroFlakesVis));
    const float microFacetVis = scene_rdl2::math::saturate(sample.microfacetBlend * (1.0f - macroFlakesVis));

    // If the 4 GGX lobes added above cover more than a certain threshold (macro flakes max threshold (default 90%),
    // and there are more flakes found in the pixel footprint, create a GGX Cook Torrance microfacet lobe
    // for remaining visibility (1.0 is full coverage) instead of a microflake lobe which is more expensive
    if (macroFlakesVis >= MACROFLAKES_MAX_THRESHOLD) {
        if (microFlakeCount > 0) {
            macroFlakesAvgColor /= static_cast<float>(macroFlakeCount);
            // If we're above 90%, we don't do microflakes - so the below lobe is a compensation term
            // However, we don't add any more lobes if macro flakes visibility is > mMacroFlakesFullThresh (99% default)
            if (macroFlakesVis < MACROFLAKES_FULL_THRESHOLD && !scene_rdl2::math::isBlack(macroFlakesAvgColor)) {
                moonray::shading::MicrofacetIsotropicBRDF microfacetRefl(refFrame.getN(),
                                                                macroFlakesAvgColor,
                                                                macroFlakesAvgColor,
                                                                mUniformParams.mFlakeRandomness,
                                                                ispc::MICROFACET_DISTRIBUTION_GGX,
                                                                ispc::MICROFACET_GEOMETRIC_SMITH);

                bsdfBuilder.addMicrofacetIsotropicBRDF(microfacetRefl,
                                                       (1.0f - macroFlakesVis) * coverageFactor * glitterMask,
                                                       combineBehavior,
                                                       label);
            }
        }
    } else { // ggxFlakesVis < mGGXMaxThreshold
        // If the macro flakes visibility is less than ggxMaxThreshold (default 90%), create
        // a StochasticFlakesBsdf lobe - considering it as microflake contribution
        scene_rdl2::alloc::Arena *arena = getArena(tls);
        scene_rdl2::math::Color avgColor = createMicroFlake(params, bsdfBuilder, microFlakeCount, macroFlakeCount, arena, flakes,
                                                refFrame, microFlakeVis, baseFlakeColors, hsvColorVariation,
                                                glitterMask, label, coverageFactor, combineBehavior);

        // Add a Cook-Torrance lobe weighted by the remaining blend weight
        if (microFacetVis > 0.0f && !isBlack(avgColor)) {
            moonray::shading::MicrofacetIsotropicBRDF microfacetRefl(refFrame.getN(),
                                                            avgColor,
                                                            avgColor,
                                                            mUniformParams.mFlakeRandomness,
                                                            ispc::MICROFACET_DISTRIBUTION_GGX,
                                                            ispc::MICROFACET_GEOMETRIC_SMITH);

            bsdfBuilder.addMicrofacetIsotropicBRDF(microfacetRefl,
                                                   microFacetVis * coverageFactor * glitterMask,
                                                   combineBehavior,
                                                   label);
        }
    }

    bsdfBuilder.endAdjacentComponents();

    return;
}

const ispc::GLITTER_Glitter*
Glitter::getIspc()
{
    return &mIspc;
}

const ispc::GLITTER_UniformParameters*
Glitter::getIspcUniformParameters() const
{
    return &mUniformParams;
}

ispc::GLITTER_UniformParameters*
Glitter::getIspcUniformParameters()
{
    return &mUniformParams;
}

}
}

