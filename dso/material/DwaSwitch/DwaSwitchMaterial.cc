// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

///
/// @file DwaSwitchMaterial.cc
/// $Id$
///

#include "attributes.cc"
#include "DwaSwitchMaterial_ispc_stubs.h"
#include "labels.h"

#include <moonshine/material/dwabase/DwaBase.h>
#include <moonray/common/mcrt_macros/moonray_static_check.h>
#include <moonray/rendering/shading/MaterialApi.h>
#include <moonshine/material/dwabase/Blending.h>

#include <string>

using namespace scene_rdl2::math;
using namespace moonray::shading;
using namespace moonshine::dwabase;

namespace {

DECLARE_DWA_BASE_LABELS()

} // end anonymous namespace

//---------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(DwaSwitchMaterial, DwaBaseLayerable)

public:
    DwaSwitchMaterial(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name);

    void resolveUniformParameters(ispc::DwaBaseUniformParameters &uParams) const override;

    virtual void update() override;

    bool getCastsCaustics() const override;

    bool hasGlitter() const override { return mMaterial->hasGlitter(); }

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
        return ispc::DwaSwitchMaterial_getCastsCausticsFunc();
    }

    void * getResolveSubsurfaceTypeISPCFunc() const override
    {
        return ispc::DwaSwitchMaterial_getResolveSubsurfaceTypeFunc();
    }

    void * getResolveParametersISPCFunc() const override
    {
        return ispc::DwaSwitchMaterial_getResolveParametersFunc();
    }

    void * getResolvePresenceISPCFunc() const override
    {
        return ispc::DwaSwitchMaterial_getResolvePresenceFunc();
    }

    void * getResolvePreventLightCullingISPCFunc() const override
    {
        return ispc::DwaSwitchMaterial_getResolvePreventLightCullingFunc();
    }

    void * getResolveSubsurfaceNormalISPCFunc() const override
    {
        return ispc::DwaSwitchMaterial_getresolveSubsurfaceNormalFunc();
    }

    EvalNormalFunc getEvalSubsurfaceNormalFunc() const override
    {
        return reinterpret_cast<EvalNormalFunc>(DwaSwitchMaterial::evalSubsurfaceNormal);
    }

    const ispc::DwaSwitchMaterial* getISPCMaterialStruct() const { return &mIspc; }

private:
    static void shade(const scene_rdl2::rdl2::Material* self,
                      TLState *tls,
                      const State& state,
                      BsdfBuilder& bsdfBuilder);

    static Vec3f evalSubsurfaceNormal(const scene_rdl2::rdl2::Material* self,
                                      TLState *tls,
                                      const State& state);

    ispc::DwaSwitchMaterial mIspc;
    const DwaBaseLayerable* mMaterial;

RDL2_DSO_CLASS_END(DwaSwitchMaterial)


//---------------------------------------------------------------------------

DwaSwitchMaterial::DwaSwitchMaterial(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name)
    : Parent(sceneClass, name, sLabels)
    , mMaterial(nullptr)
{
    mType |= scene_rdl2::rdl2::INTERFACE_DWABASELAYERABLE;

    mShadeFunc = DwaSwitchMaterial::shade;
    mShadeFuncv = (scene_rdl2::rdl2::ShadeFuncv) ispc::DwaSwitchMaterial_getShadeFunc();
    mIspc.mEvalSubsurfaceNormal = (intptr_t)DwaSwitchMaterial::evalSubsurfaceNormal;
}

void
DwaSwitchMaterial::resolveUniformParameters(ispc::DwaBaseUniformParameters &uParams) const
{
    if (mMaterial) {
        mMaterial->resolveUniformParameters(uParams);
    }
}

void
DwaSwitchMaterial::update()
{
    const int choice = get(attrChoice);
    if (choice < 0 || choice >= ispc::DWA_MAX_MATERIALS) {
        error("Out of range choice selection: ", choice,
            ".   Only values between 0 and 63 are accepted.");
        return;
    }

    SceneObject* dwaMaterials[ispc::DWA_MAX_MATERIALS];

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

    mMaterial = registerLayerable(dwaMaterials[choice], mIspc.mSubMaterial);
    resolveUniformParameters(mIspc.mUParams);
    mIspc.mSubsurfaceTraceSet = (scene_rdl2::rdl2::TraceSet *)get(attrSubsurfaceTraceSet);

}

bool
DwaSwitchMaterial::getCastsCaustics() const
{
    if (mMaterial) {
        return mMaterial->getCastsCaustics();
    } else {
        return false;
    }
}

int
DwaSwitchMaterial::resolveSubsurfaceType(const State& state) const
{
    int type = ispc::SubsurfaceType::SUBSURFACE_NONE;
    if (mMaterial) {
        type = mMaterial->resolveSubsurfaceType(state);
    }
    return type;
}

bool
DwaSwitchMaterial::resolveParameters(TLState *tls,
                                     const State& state,
                                     const bool castsCaustics,
                                     ispc::DwaBaseParameters &params) const
{
    bool result = false;
    if (mMaterial) {
        result = mMaterial->resolveParameters(tls, state, castsCaustics, params);
        // override this, to make sure *this* material's evalSubsurfaceNormal() func is called,
        // and not the child material's func.  See MOONRAY-3768
        params.mEvalSubsurfaceNormalFn = mIspc.mEvalSubsurfaceNormal;
    }
    return result;
}

float
DwaSwitchMaterial::resolvePresence(TLState *tls,
                                   const State& state) const
{
    float result = 1.f;
    if (mMaterial) {
        result = mMaterial->resolvePresence(tls, state);
    }
    return result;
}

float
DwaSwitchMaterial::resolveRefractiveIndex(TLState *tls,
                                          const State& state) const
{
    float result = 1.f;
    if (mMaterial) {
        result = mMaterial->resolveRefractiveIndex(tls, state);
    }
    return result;
}

bool
DwaSwitchMaterial::resolvePreventLightCulling(const State& state) const
{
    bool result = false;
    if (mMaterial) {
        result = mMaterial->resolvePreventLightCulling(state);
    }
    return result;
}

Vec3f
DwaSwitchMaterial::resolveSubsurfaceNormal(TLState *tls,
                                           const State& state) const

{
    Vec3f result(state.getN());
    if (mMaterial) {
        result = mMaterial->resolveSubsurfaceNormal(tls, state);
    }
    return result;
}


void
DwaSwitchMaterial::shade(const scene_rdl2::rdl2::Material* self,
                         TLState *tls,
                         const State& state,
                         BsdfBuilder& bsdfBuilder)
{
    const DwaSwitchMaterial* me = static_cast<const DwaSwitchMaterial*>(self);
    const ispc::DwaSwitchMaterial* ispc = me->getISPCMaterialStruct();
    const bool castsCaustics = me->getCastsCaustics();
    ispc::DwaBaseParameters params;
    if (me->resolveParameters(tls, state, castsCaustics, params)) {
        // get sss trace set for this DwaSwitchMaterial
        params.mSubsurfaceTraceSet = (scene_rdl2::rdl2::TraceSet *)me->get(attrSubsurfaceTraceSet);
        me->createLobes(me, tls, state, bsdfBuilder, params, ispc->mUParams, sLabels);
    }
}

Vec3f
DwaSwitchMaterial::evalSubsurfaceNormal(const scene_rdl2::rdl2::Material* self,
                                        TLState *tls,
                                        const State& state)
{
    const DwaSwitchMaterial* me = static_cast<const DwaSwitchMaterial*>(self);
    return me->resolveSubsurfaceNormal(tls, state);
}



// This is a construct to access the ISPC Data Struct from Inside the Scalar Material object.
// Note we are not using MATERIAL_GET_ISPC_CPTR to retrieve the ispc structure from the Material
// class as in other materials since that macro assumes a certain data layout we do not adhere to in
// DwaBase materials.
extern "C" {
const void*
getDwaSwitchMaterialStruct(const void* ptr)
{
    const DwaSwitchMaterial* classPtr = static_cast<const DwaSwitchMaterial*>(ptr);
    return (static_cast<const void*>(classPtr->getISPCMaterialStruct()));
}
}

//---------------------------------------------------------------------------


