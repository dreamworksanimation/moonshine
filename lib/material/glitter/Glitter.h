// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file Glitter.h

#pragma once

#include "Glitter_ispc_stubs.h"

#include <moonray/rendering/shading/MaterialApi.h>
#include <moonray/rendering/shading/BasicTexture.h>
#include <moonray/rendering/bvh/shading/Xform.h>
#include <scene_rdl2/scene/rdl2/Material.h>
#include <scene_rdl2/render/logging/logging.h>
#include <scene_rdl2/common/math/Color.h>
#include <scene_rdl2/common/math/Vec3.h>
#include <scene_rdl2/common/math/ReferenceFrame.h>
#include <moonshine/common/noise/Worley.h>

namespace moonshine {

namespace glitter {

static const unsigned int sMaxMacroFlakeCount = 4;
static const int sGlitterFlakeTextureCount = 2;

static const std::string sResultCodeSuccess = "";
static const std::string sResultCodeNoRefN =
        "Unable to acquire refN which is required for glitter. "
        "Glitter cannot be applied";
static const std::string sResultCodeNoRefPPartials =
        "No partial derivatives associated with refP. "
        "Unable to compute deformation for 'deformation compensation' feature. "
        "Glitter may stretch";

// convenience function
finline const std::string&
getResultCodeMessage(ispc::GLITTER_ResultCode resultCode)
{
    switch (resultCode) {
    case ispc::GLITTER_RESULTCODE_NO_REFN:
        return sResultCodeNoRefN;
    case ispc::GLITTER_RESULTCODE_NO_REFP_PARTIALS:
        return sResultCodeNoRefPPartials;
    default:
        return sResultCodeSuccess;
    }
}

typedef std::unique_ptr<moonray::shading::BasicTexture>   TexturePointer;
typedef std::pair<std::string, float>                   GlitterTexture;

class Glitter
{
public:
    Glitter(const scene_rdl2::rdl2::Material* shader,
            const ispc::GLITTER_UniformParameters& params);

    // Textured Glitter Flakes
    Glitter(scene_rdl2::rdl2::Material* shader,
            const std::vector<GlitterTexture>& filenames,
            scene_rdl2::rdl2::ShaderLogEventRegistry& logEventRegistry,
            const ispc::GLITTER_UniformParameters& params);

    // Creates all the lobes for glitter and populates them in the bsdf container
    void createLobes(moonray::shading::TLState* tls,
                     const moonray::shading::State& state,
                     moonray::shading::BsdfBuilder& bsdfBuilder,
                     const ispc::GLITTER_VaryingParameters& params,
                     const int label,
                     ispc::GLITTER_ResultCode& resultCode,
                     const scene_rdl2::rdl2::LightSet* lightSet) const;

    // Call this function to visualize the LOD modes
    void createDebugBlendLobes(moonray::shading::TLState *tls,
                               const moonray::shading::State& state,
                               moonray::shading::BsdfBuilder& bsdfBuilder,
                               const ispc::GLITTER_VaryingParameters& params,
                               ispc::GLITTER_ResultCode& resultCode) const;

    // Call this function to visualize flake colors as emission
    void createDebugColorLobes(moonray::shading::TLState *tls,
                               const moonray::shading::State& state,
                               moonray::shading::BsdfBuilder& bsdfBuilder,
                               const ispc::GLITTER_VaryingParameters& params,
                               ispc::GLITTER_ResultCode& resultCode) const;

    // Call this function to visualize average flake color as emission
    void createDebugAverageColorLobes(moonray::shading::TLState *tls,
                                      const moonray::shading::State& state,
                                      moonray::shading::BsdfBuilder& bsdfBuilder,
                                      const ispc::GLITTER_VaryingParameters& params) const;

    // Call this function to visualize footprintArea 
    void createDebugSampleLobes(moonray::shading::TLState *tls,
                                const moonray::shading::State& state,
                                moonray::shading::BsdfBuilder& bsdfBuilder,
                                const ispc::GLITTER_VaryingParameters& params,
                                const int mode,
                                ispc::GLITTER_ResultCode& resultCode) const;

    // Functions to get ispc pointers to exported structures
    const ispc::GLITTER_Glitter* getIspc();
    const ispc::GLITTER_UniformParameters* getIspcUniformParameters() const;
    ispc::GLITTER_UniformParameters* getIspcUniformParameters();

private:
    bool initializeNoiseSample(ispc::NOISE_WorleySample& noiseSample,
                               moonray::shading::TLState *tls,
                               const moonray::shading::State& state,
                               const ispc::GLITTER_VaryingParameters& params,
                               ispc::GLITTER_ResultCode& resultCode) const;

    bool calculateDeformationFactors(const moonray::shading::State& state,
                                     const bool compensateDeformation,
                                     ispc::NOISE_WorleySample& sample,
                                     ispc::GLITTER_ResultCode& resultCode) const;

    static unsigned int finalizeFlakes(noise::Worley_PointArray::iterator flkItrBeg,
                                       noise::Worley_PointArray::iterator flkItrCur);

    unsigned int findNearestFlakes(const ispc::NOISE_WorleySample& sample,
                                   const noise::Flake_StyleArray& styleCDF,
                                   const noise::Flake_StyleArray& styleSizes,
                                   float flakeDensity,
                                   float flakeJitter,
                                   noise::Worley_PointArray& flakes) const;

    scene_rdl2::math::Color computeFlakeColor(const int flakeID,
                                         const scene_rdl2::math::Color& baseColor,
                                         const scene_rdl2::math::Vec3f& colorVariation) const;

    scene_rdl2::math::Color computeAverageFlakeColor(const scene_rdl2::math::Color& baseColor,
                                                const scene_rdl2::math::Vec3f& hsvVariation,
                                                const float coverageFactor) const;
    scene_rdl2::math::Color computeAverageBaseColor(const ispc::GLITTER_VaryingParameters& params) const;


    scene_rdl2::math::Color createMacroFlakes(moonray::shading::BsdfBuilder &builder,
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
                                         bool& singleMacroFlake,
                                         const scene_rdl2::rdl2::LightSet* lightSet) const;

    void readFlakeTexturesAndModifyWeights(moonray::shading::TLState* tls,
                                           const moonray::shading::State& state,
                                           const unsigned int macroFlakeCount,
                                           const float flakeOrientationRandomness,
                                           noise::Worley_PointArray& flakes,
                                           std::array<scene_rdl2::math::Color, sMaxMacroFlakeCount>& flakeTextures) const;

    void computeStyleCDF(const noise::Flake_StyleArray& styleFrequencies,
                         noise::Flake_StyleArray& styleCDF) const;

    void setFlakeStyles(const ispc::GLITTER_VaryingParameters& params,
                        const unsigned int macroFlakeCount,
                        const noise::Worley_PointArray& flakes,
                        std::array<scene_rdl2::math::Color, sMaxMacroFlakeCount>& flakeColors,
                        std::array<float, sMaxMacroFlakeCount>& flakeRoughnesses) const;

    int  chooseFlakePattern(int id) const;
    void rotateFlakeUVs(int id,
                        const float flakeOrientationRandomness,
                        scene_rdl2::math::Vec2f& uv) const;

    scene_rdl2::math::Color createMicroFlake(const ispc::GLITTER_VaryingParameters& params,
                                        moonray::shading::BsdfBuilder &builder,
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
                                        const ispc::BsdfBuilderBehavior combineBehavior,
                                        const scene_rdl2::rdl2::LightSet* lightSet) const;

    ispc::GLITTER_Glitter mIspc;
    ispc::GLITTER_UniformParameters mUniformParams;

    std::unique_ptr<moonray::shading::Xform> mXform;
    std::unique_ptr<noise::Worley> mNoiseWorley;

    // Individual Flake Textures & their weights - used in macroflake mode
    std::array<TexturePointer, sGlitterFlakeTextureCount> mFlakePatterns;
};

} // namespace glitter
} // namespace moonshine



