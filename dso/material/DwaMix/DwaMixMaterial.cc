// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

///
/// @file DwaMixMaterial.cc
/// $Id$
///

#include "attributes.cc"
#include "DwaMixMaterial_ispc_stubs.h"
#include "labels.h"

#include <moonshine/material/dwabase/DwaBase.h>
#include <moonray/common/mcrt_macros/moonray_static_check.h>
#include <moonray/rendering/shading/MaterialApi.h>
#include <moonshine/material/dwabase/Blending.h>

#include <string>

#define DWA_MIX_FLOAT_PAD 0.001f

using namespace scene_rdl2::math;
using namespace moonray::shading;
using namespace moonshine::dwabase;

namespace {

DECLARE_DWA_BASE_LABELS()

DwaBaseAttributeKeys
collectAttributeKeys()
{
    DwaBaseAttributeKeys keys;

    // These parameters exist for when both input materials are glitter
    // in which case we construct a Glitter object that is owned by this
    // layer material and overrides the uniform parameters of the input
    // materials glitter.
    keys.mGlitterSeed                           = attrFallbackGlitterSeed;
    keys.mGlitterSpace                          = attrFallbackGlitterSpace;
    keys.mGlitterRandomness                     = attrFallbackGlitterRandomness;
    keys.mGlitterStyleAFrequency                = attrFallbackGlitterStyleAFrequency;
    keys.mGlitterFlakeTextureA                  = attrFallbackGlitterTextureA;
    keys.mGlitterStyleBFrequency                = attrFallbackGlitterStyleBFrequency;
    keys.mGlitterFlakeTextureB                  = attrFallbackGlitterTextureB;
    keys.mGlitterDenseLodQuality                = attrFallbackGlitterLodQuality;
    keys.mGlitterLayeringMode                   = attrFallbackGlitterLayeringMode;
    keys.mGlitterDebugMode                      = attrFallbackGlitterDebugMode;

    return keys;
}

float
interpolateMix(float mix, ispc::MixInterpolation mode, int inputMultiplier) {
    mix = mix * inputMultiplier;
    switch (mode) {
    case ispc::DWA_MIX_LINEAR:
        break;
    case ispc::DWA_MIX_HOLD:
        mix = scene_rdl2::math::floor(mix);
        break;
    case ispc::DWA_MIX_NEAREST:
        mix = scene_rdl2::math::floor(mix + 0.5f);
        break;
    case ispc::DWA_MIX_SMOOTH:
        mix = (scene_rdl2::math::sin((mix + 0.5f) * sTwoPi) +
                                     (mix + 0.5f) * sTwoPi) * sOneOverTwoPi - 0.5f;
        break;
    default:
        break;
    }
    // Create "flat" spot around each whole number to account for floating point
    // error. Particularly important to ensure we hit the last or first material
    // or if a specific whole float value is provided
    if (scene_rdl2::math::abs(static_cast<int>(mix + 0.5f) - mix) < DWA_MIX_FLOAT_PAD) {
        mix = static_cast<int>(mix + 0.5f);
    }

    return mix;
}

} // end anonymous namespace

//---------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(DwaMixMaterial, DwaBase)

public:
    DwaMixMaterial(const SceneClass& sceneClass, const std::string& name);

    void resolveUniformParameters(ispc::DwaBaseUniformParameters &uParams) const override;

    virtual void update() override;

    bool hasGlitter() const override;

    bool getCastsCaustics() const override;

    int resolveSubsurfaceType(const State& state) const override;

    bool resolveParameters(TLState *tls,
                           const State& state,
                           bool castsCaustics,
                           ispc::DwaBaseParameters &params) const override;

    Vec3f resolveSubsurfaceNormal(TLState *tls,
                                  const State& state) const override;

    float resolvePresence(TLState *tls,
                          const State& state) const override;

    float resolveRefractiveIndex(TLState *tls,
                                 const State& state) const override;

    bool resolvePreventLightCulling(const State& state) const override;

    void * getCastsCausticsISPCFunc() const override
    {
        return ispc::DwaMixMaterial_getCastsCausticsFunc();
    }

    void * getResolveSubsurfaceTypeISPCFunc() const override
    {
        return ispc::DwaMixMaterial_getResolveSubsurfaceTypeFunc();
    }

    void * getResolveParametersISPCFunc() const override
    {
        return ispc::DwaMixMaterial_getResolveParametersFunc();
    }

    void * getResolvePresenceISPCFunc() const override
    {
        return ispc::DwaMixMaterial_getResolvePresenceFunc();
    }

    void * getResolvePreventLightCullingISPCFunc() const override
    {
        return ispc::DwaMixMaterial_getResolvePreventLightCullingFunc();
    }

    void * getResolveSubsurfaceNormalISPCFunc() const override
    {
        return ispc::DwaMixMaterial_getresolveSubsurfaceNormalFunc();
    }

    EvalNormalFunc getEvalSubsurfaceNormalFunc() const override
    {
        return reinterpret_cast<EvalNormalFunc>(DwaMixMaterial::evalSubsurfaceNormal);
    }

    const ispc::DwaMixMaterial* getISPCMixMaterialStruct() const { return &mIspc; }

private:
    static void shade(const Material* self,
                      TLState *tls,
                      const State& state,
                      BsdfBuilder& bsdfBuilder);

    static Vec3f evalSubsurfaceNormal(const Material* self,
                                      TLState *tls,
                                      const State& state);

    ispc::DwaMixMaterial mIspc;
    unsigned int mGlitterCount;

RDL2_DSO_CLASS_END(DwaMixMaterial)


//---------------------------------------------------------------------------

DwaMixMaterial::DwaMixMaterial(const SceneClass& sceneClass, const std::string& name)
    : Parent(sceneClass, name,
             collectAttributeKeys(),
             ispc::DwaMixMaterial_collectAttributeFuncs(),
             sLabels,
             ispc::Model::Layer)
{
    mType |= INTERFACE_DWABASELAYERABLE;

    mShadeFunc = DwaMixMaterial::shade;
    mShadeFuncv = (ShadeFuncv) ispc::DwaMixMaterial_getShadeFunc();
    mIspc.mEvalSubsurfaceNormalFn = (intptr_t)DwaMixMaterial::evalSubsurfaceNormal;

    // This is used to get the glitter pointer and uniform parameters in ispc
    mIspc.mDwaBase = getISPCBaseMaterialStruct();
}

void
DwaMixMaterial::resolveUniformParameters(ispc::DwaBaseUniformParameters &uParams) const
{
    if (mIspc.mSubMaterials[0]) {
        size_t i = 0;
        DwaBaseLayerable* currSubMaterial = reinterpret_cast<DwaBaseLayerable*>(mIspc.mSubMaterials[i]);
        DwaBaseLayerable* prevSubMaterial = nullptr;
        while (currSubMaterial) {
            ispc::DwaBaseUniformParameters uParamsCurr, uParamsPrev;
            currSubMaterial->resolveUniformParameters(uParamsCurr);
            if (prevSubMaterial) {
                prevSubMaterial->resolveUniformParameters(uParamsPrev);

                blendUniformParameters(uParamsCurr,
                                       uParamsPrev,
                                       uParams,
                                       get(attrFallbackSpecularModel),
                                       get(attrFallbackOuterSpecularModel),
                                       get(attrFallbackOuterSpecularUseBending),
                                       get(attrFallbackBSSRDF),
                                       get(attrFallbackThinGeometry),
                                       get(attrFallbackPreventLightCulling));
            } else {
                uParams = uParamsCurr;
            }

            prevSubMaterial = currSubMaterial;
            currSubMaterial = reinterpret_cast<DwaBaseLayerable*>(mIspc.mSubMaterials[++i]);
        }
    }
}

void
DwaMixMaterial::update()
{
    SceneObject* dwaMaterials[ispc::DWA_MIX_MAX_MATERIALS];

    dwaMaterials[0] = get(attrMaterial0);
    dwaMaterials[1] = get(attrMaterial1);
    dwaMaterials[2] = get(attrMaterial2);
    dwaMaterials[3] = get(attrMaterial3);
    dwaMaterials[4] = get(attrMaterial4);
    dwaMaterials[5] = get(attrMaterial5);
    dwaMaterials[6] = get(attrMaterial6);
    dwaMaterials[7] = get(attrMaterial7);
    dwaMaterials[8] = get(attrMaterial8);
    dwaMaterials[9] = get(attrMaterial9);

    dwaMaterials[10] = get(attrMaterial10);
    dwaMaterials[11] = get(attrMaterial11);
    dwaMaterials[12] = get(attrMaterial12);
    dwaMaterials[13] = get(attrMaterial13);
    dwaMaterials[14] = get(attrMaterial14);
    dwaMaterials[15] = get(attrMaterial15);
    dwaMaterials[16] = get(attrMaterial16);
    dwaMaterials[17] = get(attrMaterial17);
    dwaMaterials[18] = get(attrMaterial18);
    dwaMaterials[19] = get(attrMaterial19);

    dwaMaterials[20] = get(attrMaterial20);
    dwaMaterials[21] = get(attrMaterial21);
    dwaMaterials[22] = get(attrMaterial22);
    dwaMaterials[23] = get(attrMaterial23);
    dwaMaterials[24] = get(attrMaterial24);
    dwaMaterials[25] = get(attrMaterial25);
    dwaMaterials[26] = get(attrMaterial26);
    dwaMaterials[27] = get(attrMaterial27);
    dwaMaterials[28] = get(attrMaterial28);
    dwaMaterials[29] = get(attrMaterial29);

    dwaMaterials[30] = get(attrMaterial30);
    dwaMaterials[31] = get(attrMaterial31);
    dwaMaterials[32] = get(attrMaterial32);
    dwaMaterials[33] = get(attrMaterial33);
    dwaMaterials[34] = get(attrMaterial34);
    dwaMaterials[35] = get(attrMaterial35);
    dwaMaterials[36] = get(attrMaterial36);
    dwaMaterials[37] = get(attrMaterial37);
    dwaMaterials[38] = get(attrMaterial38);
    dwaMaterials[39] = get(attrMaterial39);

    dwaMaterials[40] = get(attrMaterial40);
    dwaMaterials[41] = get(attrMaterial41);
    dwaMaterials[42] = get(attrMaterial42);
    dwaMaterials[43] = get(attrMaterial43);
    dwaMaterials[44] = get(attrMaterial44);
    dwaMaterials[45] = get(attrMaterial45);
    dwaMaterials[46] = get(attrMaterial46);
    dwaMaterials[47] = get(attrMaterial47);
    dwaMaterials[48] = get(attrMaterial48);
    dwaMaterials[49] = get(attrMaterial49);

    dwaMaterials[50] = get(attrMaterial50);
    dwaMaterials[51] = get(attrMaterial51);
    dwaMaterials[52] = get(attrMaterial52);
    dwaMaterials[53] = get(attrMaterial53);
    dwaMaterials[54] = get(attrMaterial54);
    dwaMaterials[55] = get(attrMaterial55);
    dwaMaterials[56] = get(attrMaterial56);
    dwaMaterials[57] = get(attrMaterial57);
    dwaMaterials[58] = get(attrMaterial58);
    dwaMaterials[59] = get(attrMaterial59);

    dwaMaterials[60] = get(attrMaterial60);
    dwaMaterials[61] = get(attrMaterial61);
    dwaMaterials[62] = get(attrMaterial62);
    dwaMaterials[63] = get(attrMaterial63);

    mIspc.mColorSpace = static_cast<ispc::BlendColorSpace>(get(attrColorSpace));

    // Register all attached materials.
    // If SceneObject* is a DwaBaseLayerable* register the layerable.
    // Fill in the SubMaterialData for all attached materials.
    // At shade time we can index in with the 'mix' value and choose the appropriate 2 materials
    // to blend between since we can't determine the 2 materials here because 'mix' is bindable
    // and varies per shading point.
    mGlitterCount = 0;
    mIspc.mCastsCaustics = false;
    size_t i = 0;
    while (i < ispc::DWA_MIX_MAX_MATERIALS) {
        mIspc.mSubMaterials[i] = (intptr_t)registerLayerable(dwaMaterials[i], mIspc.mSubMaterialData[i]);
        if (mIspc.mSubMaterials[i]) {
            if (mIspc.mSubMaterialData[i].mHasGlitter) mGlitterCount++;
            // reinterpret_cast should be ok because registerLayerable() performs dynamic_cast
            // and the nullptr check is performed before entering this scope so at this point
            // mIspc.mSubMaterials[i] should be a DwaBaseLayerable*
            const DwaBaseLayerable* currSubMaterial = reinterpret_cast<DwaBaseLayerable*>(mIspc.mSubMaterials[i]);
            if (currSubMaterial->getCastsCaustics()) {
                // If any attached material casts caustics, they all should.
                mIspc.mCastsCaustics = true;
            }
        } else {
            // Warning message is printed by SceneObject if the bound
            // interface type is incorrect.
            break;
        }
        ++i;
    }
    unsigned int numValidInputs = i;

    resolveUniformParameters(mIspc.mUParams);

    // If more than one input material has glitter enabled then we construct
    // a new Glitter object which is used to override the input materials'
    // uniform glitter parameters.   This is needed because we create one
    // set of blended glitter lobes and because the uniform parameters
    // are set at construction time they can't be blended at shade time.
    if (mGlitterCount > 1) {
        updateGlitter();
    }

    if (get(attrRemapMixToInputs)) {
        mIspc.mInputMultiplier = numValidInputs - 1;
        mIspc.mMaxMixValue = 1.f;
    } else {
        mIspc.mInputMultiplier = 1;
        mIspc.mMaxMixValue = static_cast<float>(numValidInputs - 1);
    }

    mIspc.mMixInterpolation = static_cast<ispc::MixInterpolation>(get(attrMixInterpolation));

    mIspc.mSubsurfaceTraceSet = static_cast<TraceSet *>(get(attrSubsurfaceTraceSet));
}

bool
DwaMixMaterial::hasGlitter() const
{
    return mGlitterCount > 0;
}

bool
DwaMixMaterial::getCastsCaustics() const
{
    return mIspc.mCastsCaustics;
}

int
DwaMixMaterial::resolveSubsurfaceType(const State& state) const
{
    return mIspc.mDwaBase->mUParams.mSubsurface;
}

bool
DwaMixMaterial::resolveParameters(TLState *tls,
                                  const State& state,
                                  const bool castsCaustics,
                                  ispc::DwaBaseParameters &params) const
{
    float mix = clamp(evalFloat(this, attrMix, tls, state), 0.f, mIspc.mMaxMixValue);
    mix = interpolateMix(mix, mIspc.mMixInterpolation, mIspc.mInputMultiplier);
    const unsigned int t0 = static_cast<int>(mix);
    const DwaBaseLayerable* material0 = reinterpret_cast<DwaBaseLayerable*>(mIspc.mSubMaterials[t0]);
    const DwaBaseLayerable* material1 = reinterpret_cast<DwaBaseLayerable*>(mIspc.mSubMaterials[t0 + 1]);

    return blendParameters(tls,
                           state,
                           castsCaustics,
                           params,
                           mIspc.mDwaBase->mUParams,
                           mIspc.mColorSpace,
                           getGlitterPointer(),
                           mIspc.mEvalSubsurfaceNormalFn,
                           mIspc.mSubMaterialData[t0].mHasGlitter,
                           mIspc.mSubMaterialData[t0 + 1].mHasGlitter,
                           material0,
                           material1,
                           mix - t0);
}

bool
DwaMixMaterial::resolvePreventLightCulling(const State& state) const
{
    return mIspc.mDwaBase->mUParams.mPreventLightCulling;
}

float
DwaMixMaterial::resolvePresence(TLState *tls,
                                const State& state) const
{
    float mix = clamp(evalFloat(this, attrMix, tls, state), 0.f, mIspc.mMaxMixValue);
    mix = interpolateMix(mix, mIspc.mMixInterpolation, mIspc.mInputMultiplier);
    const unsigned int t0 = static_cast<int>(mix);
    const DwaBaseLayerable* material0 = reinterpret_cast<DwaBaseLayerable*>(mIspc.mSubMaterials[t0]);
    const DwaBaseLayerable* material1 = reinterpret_cast<DwaBaseLayerable*>(mIspc.mSubMaterials[t0 + 1]);

    return blendPresence(tls,
                         state,
                         material0,
                         material1,
                         mix - t0);
}

float
DwaMixMaterial::resolveRefractiveIndex(TLState *tls,
                                       const State& state) const
{
    float mix = clamp(evalFloat(this, attrMix, tls, state), 0.f, mIspc.mMaxMixValue);
    mix = interpolateMix(mix, mIspc.mMixInterpolation, mIspc.mInputMultiplier);
    const unsigned int t0 = static_cast<int>(mix);
    const DwaBaseLayerable* material0 = reinterpret_cast<DwaBaseLayerable*>(mIspc.mSubMaterials[t0]);
    const DwaBaseLayerable* material1 = reinterpret_cast<DwaBaseLayerable*>(mIspc.mSubMaterials[t0 + 1]);

    return blendRefractiveIndex(tls,
                                state,
                                material0,
                                material1,
                                mix - t0);
}

Vec3f
DwaMixMaterial::resolveSubsurfaceNormal(TLState *tls,
                                        const State& state) const

{
    float mix = clamp(evalFloat(this, attrMix, tls, state), 0.f, mIspc.mMaxMixValue);
    mix = interpolateMix(mix, mIspc.mMixInterpolation, mIspc.mInputMultiplier);
    const unsigned int t0 = static_cast<int>(mix);
    const DwaBaseLayerable* material0 = reinterpret_cast<DwaBaseLayerable*>(mIspc.mSubMaterials[t0]);
    const DwaBaseLayerable* material1 = reinterpret_cast<DwaBaseLayerable*>(mIspc.mSubMaterials[t0 + 1]);

    return blendSubsurfaceNormal(tls,
                                 state,
                                 material0,
                                 material1,
                                 mix - t0);
}


void
DwaMixMaterial::shade(const Material* self,
                      TLState *tls,
                      const State& state,
                      BsdfBuilder& bsdfBuilder)
{
    const DwaMixMaterial* me = static_cast<const DwaMixMaterial*>(self);
    const ispc::DwaMixMaterial* ispc = me->getISPCMixMaterialStruct();

    const bool castsCaustics = me->getCastsCaustics();

    ispc::DwaBaseParameters params;
    if (me->resolveParameters(tls, state, castsCaustics, params)) {
        // get sss trace set for this DwaMixMaterial
        params.mSubsurfaceTraceSet = static_cast<TraceSet *>(me->get(attrSubsurfaceTraceSet));
        me->createLobes(me, tls, state, bsdfBuilder, params, ispc->mUParams, sLabels);
    }
}

Vec3f
DwaMixMaterial::evalSubsurfaceNormal(const Material* self,
                                     TLState *tls,
                                     const State& state)
{
    const DwaMixMaterial* me = static_cast<const DwaMixMaterial*>(self);
    return me->resolveSubsurfaceNormal(tls, state);
}



// This is a construct to access the ISPC Data Struct from Inside the Scalar Material object.
// Note we are not using MATERIAL_GET_ISPC_CPTR to retrieve the ispc structure from the Material
// class as in other materials since that macro assumes a certain data layout we do not adhere to in
// DwaBase materials.
extern "C" {
const void*
getDwaMixMaterialStruct(const void* ptr)
{
    const DwaMixMaterial* classPtr = static_cast<const DwaMixMaterial*>(ptr);
    return (static_cast<const void*>(classPtr->getISPCMixMaterialStruct()));
}
}

//---------------------------------------------------------------------------


