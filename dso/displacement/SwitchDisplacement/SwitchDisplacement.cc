// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.cc"
#include "SwitchDisplacement_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>

#include <string>

using namespace scene_rdl2::math;
using namespace moonray::shading;

//---------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(SwitchDisplacement, scene_rdl2::rdl2::Displacement)

public:
    SwitchDisplacement(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name);
    ~SwitchDisplacement();
    virtual void update();

private:
    static void displace(const Displacement *self, moonray::shading::TLState *tls,
                         const State &state, Vec3f *displace);
    
    ispc::SwitchDisplacement mIspc;

RDL2_DSO_CLASS_END(SwitchDisplacement)


//---------------------------------------------------------------------------

SwitchDisplacement::SwitchDisplacement(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name) :
    Parent(sceneClass, name)
{
    mDisplaceFunc = SwitchDisplacement::displace;
    mDisplaceFuncv = (scene_rdl2::rdl2::DisplaceFuncv) ispc::SwitchDisplacement_getDisplaceFunc();
    for (size_t i = 0; i < ispc::MAX_DISP; ++i) {
        mIspc.mDisplacement[i] = 0;
    }
}

SwitchDisplacement::~SwitchDisplacement()
{
}

void
SwitchDisplacement::update()
{
    mIspc.mDisplacement[0] = (intptr_t)get(attrDisplacement0);
    mIspc.mDisplacement[1] = (intptr_t)get(attrDisplacement1);
    mIspc.mDisplacement[2] = (intptr_t)get(attrDisplacement2);
    mIspc.mDisplacement[3] = (intptr_t)get(attrDisplacement3);
    mIspc.mDisplacement[4] = (intptr_t)get(attrDisplacement4);
    mIspc.mDisplacement[5] = (intptr_t)get(attrDisplacement5);
    mIspc.mDisplacement[6] = (intptr_t)get(attrDisplacement6);
    mIspc.mDisplacement[7] = (intptr_t)get(attrDisplacement7);
    mIspc.mDisplacement[8] = (intptr_t)get(attrDisplacement8);
    mIspc.mDisplacement[9] = (intptr_t)get(attrDisplacement9);

    mIspc.mDisplacement[10] = (intptr_t)get(attrDisplacement10);
    mIspc.mDisplacement[11] = (intptr_t)get(attrDisplacement11);
    mIspc.mDisplacement[12] = (intptr_t)get(attrDisplacement12);
    mIspc.mDisplacement[13] = (intptr_t)get(attrDisplacement13);
    mIspc.mDisplacement[14] = (intptr_t)get(attrDisplacement14);
    mIspc.mDisplacement[15] = (intptr_t)get(attrDisplacement15);
    mIspc.mDisplacement[16] = (intptr_t)get(attrDisplacement16);
    mIspc.mDisplacement[17] = (intptr_t)get(attrDisplacement17);
    mIspc.mDisplacement[18] = (intptr_t)get(attrDisplacement18);
    mIspc.mDisplacement[19] = (intptr_t)get(attrDisplacement19);

    mIspc.mDisplacement[20] = (intptr_t)get(attrDisplacement20);
    mIspc.mDisplacement[21] = (intptr_t)get(attrDisplacement21);
    mIspc.mDisplacement[22] = (intptr_t)get(attrDisplacement22);
    mIspc.mDisplacement[23] = (intptr_t)get(attrDisplacement23);
    mIspc.mDisplacement[24] = (intptr_t)get(attrDisplacement24);
    mIspc.mDisplacement[25] = (intptr_t)get(attrDisplacement25);
    mIspc.mDisplacement[26] = (intptr_t)get(attrDisplacement26);
    mIspc.mDisplacement[27] = (intptr_t)get(attrDisplacement27);
    mIspc.mDisplacement[28] = (intptr_t)get(attrDisplacement28);
    mIspc.mDisplacement[29] = (intptr_t)get(attrDisplacement29);

    mIspc.mDisplacement[30] = (intptr_t)get(attrDisplacement30);
    mIspc.mDisplacement[31] = (intptr_t)get(attrDisplacement31);
    mIspc.mDisplacement[32] = (intptr_t)get(attrDisplacement32);
    mIspc.mDisplacement[33] = (intptr_t)get(attrDisplacement33);
    mIspc.mDisplacement[34] = (intptr_t)get(attrDisplacement34);
    mIspc.mDisplacement[35] = (intptr_t)get(attrDisplacement35);
    mIspc.mDisplacement[36] = (intptr_t)get(attrDisplacement36);
    mIspc.mDisplacement[37] = (intptr_t)get(attrDisplacement37);
    mIspc.mDisplacement[38] = (intptr_t)get(attrDisplacement38);
    mIspc.mDisplacement[39] = (intptr_t)get(attrDisplacement39);

    mIspc.mDisplacement[40] = (intptr_t)get(attrDisplacement40);
    mIspc.mDisplacement[41] = (intptr_t)get(attrDisplacement41);
    mIspc.mDisplacement[42] = (intptr_t)get(attrDisplacement42);
    mIspc.mDisplacement[43] = (intptr_t)get(attrDisplacement43);
    mIspc.mDisplacement[44] = (intptr_t)get(attrDisplacement44);
    mIspc.mDisplacement[45] = (intptr_t)get(attrDisplacement45);
    mIspc.mDisplacement[46] = (intptr_t)get(attrDisplacement46);
    mIspc.mDisplacement[47] = (intptr_t)get(attrDisplacement47);
    mIspc.mDisplacement[48] = (intptr_t)get(attrDisplacement48);
    mIspc.mDisplacement[49] = (intptr_t)get(attrDisplacement49);

    mIspc.mDisplacement[50] = (intptr_t)get(attrDisplacement50);
    mIspc.mDisplacement[51] = (intptr_t)get(attrDisplacement51);
    mIspc.mDisplacement[52] = (intptr_t)get(attrDisplacement52);
    mIspc.mDisplacement[53] = (intptr_t)get(attrDisplacement53);
    mIspc.mDisplacement[54] = (intptr_t)get(attrDisplacement54);
    mIspc.mDisplacement[55] = (intptr_t)get(attrDisplacement55);
    mIspc.mDisplacement[56] = (intptr_t)get(attrDisplacement56);
    mIspc.mDisplacement[57] = (intptr_t)get(attrDisplacement57);
    mIspc.mDisplacement[58] = (intptr_t)get(attrDisplacement58);
    mIspc.mDisplacement[59] = (intptr_t)get(attrDisplacement59);

    mIspc.mDisplacement[60] = (intptr_t)get(attrDisplacement60);
    mIspc.mDisplacement[61] = (intptr_t)get(attrDisplacement61);
    mIspc.mDisplacement[62] = (intptr_t)get(attrDisplacement62);
    mIspc.mDisplacement[63] = (intptr_t)get(attrDisplacement63);

    for (size_t i = 0; i < ispc::MAX_DISP; ++i) {

        scene_rdl2::rdl2::Displacement* dsp =
                reinterpret_cast<scene_rdl2::rdl2::Displacement*>(mIspc.mDisplacement[i]);
        mIspc.mDisplacementFunc[i] =
                (dsp != nullptr) ? (intptr_t) dsp->mDisplaceFuncv : (intptr_t) nullptr;
    }

}

void SwitchDisplacement::displace(const Displacement *self, moonray::shading::TLState *tls,
                                  const State &state, Vec3f *displace)
{
    const SwitchDisplacement* me = static_cast<const SwitchDisplacement*>(self);
    unsigned int choice =
            static_cast<unsigned int>(evalFloat(me, attrChoice, tls, state));

    // cycle through choices
    choice = choice % ispc::MAX_DISP;

    // default
    *displace = Vec3f(0.0f, 0.0f, 0.0f);

    const scene_rdl2::rdl2::Displacement* dsp =
            reinterpret_cast<scene_rdl2::rdl2::Displacement*>(me->mIspc.mDisplacement[choice]);
    if (dsp) {
        moonray::shading::displace(dsp, tls, state, displace);
    }
}


//---------------------------------------------------------------------------

