// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


// auto-generated
#include "attributes.cc"
#include "TemplateMap_ispc_stubs.h"

#include <scene_rdl2/scene/rdl2/rdl2.h>

//---------------------------------------------------------------------------
RDL2_DSO_CLASS_BEGIN(TemplateMap, scene_rdl2::rdl2::Map)
public:
    TemplateMap(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name);
    ~TemplateMap();
    virtual void update();

private:
RDL2_DSO_CLASS_END(TemplateMap)
//---------------------------------------------------------------------------

TemplateMap::TemplateMap(const scene_rdl2::rdl2::SceneClass &sceneClass, const std::string &name) :
    Parent(sceneClass, name)
{
    // set the ISPC vectorized sample functions
    // there are 2, one that supports derivatives, and one that does not
    mSampleFuncv = (scene_rdl2::rdl2::SampleFuncv) ispc::TemplateMap_getSampleFunc();
}

TemplateMap::~TemplateMap()
{
}

void
TemplateMap::update()
{
    // update is called once per frame when scene scene data
    // has changed.
}


