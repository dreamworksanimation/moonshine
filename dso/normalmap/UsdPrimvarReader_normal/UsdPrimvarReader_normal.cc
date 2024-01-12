// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file UsdPrimvarReader_normal.cc

#include "attributes.cc"
#include "UsdPrimvarReader_normal_ispc_stubs.h"

#include <moonray/map/primvar/Primvar.h>

#include <moonray/rendering/shading/MapApi.h>

#include <memory>

using namespace moonray::shading;
using namespace scene_rdl2::math;

//----------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(UsdPrimvarReader_normal, NormalMap)

public:
    UsdPrimvarReader_normal(const SceneClass &sceneClass,
                            const std::string &name);
    ~UsdPrimvarReader_normal() override;
    void update() override;

private:
    static void sampleNormal(const NormalMap *self,
                             moonray::shading::TLState *tls,
                             const moonray::shading::State &state,
                             Vec3f *sample);

    ispc::UsdPrimvarReader mIspc; // must be first member
    std::unique_ptr<moonray::shading::Xform> mXform;

RDL2_DSO_CLASS_END(UsdPrimvarReader_normal)

//----------------------------------------------------------------------------

UsdPrimvarReader_normal::UsdPrimvarReader_normal(const SceneClass &sceneClass, const std::string &name):
    Parent(sceneClass, name)
{
    mSampleNormalFunc = UsdPrimvarReader_normal::sampleNormal;
    mSampleNormalFuncv = (SampleNormalFuncv)ispc::UsdPrimvarReader_normal_getSampleFunc();
}

UsdPrimvarReader_normal::~UsdPrimvarReader_normal()
{
}

void
UsdPrimvarReader_normal::update()
{
    if (hasChanged(attrVarName)) {
        mOptionalAttributes.clear();
        const std::string& varname = get(attrVarName);
        // We handle these special case "varname" values:
        //      "N"
        //      "Ng"
        //      "surface_N"
        if (varname == "N") {
            mIspc.mAttributeMapType = ispc::PRIMVAR_MAP_TYPE_N;
        } else if (varname == "Ng") {
            mIspc.mAttributeMapType = ispc::PRIMVAR_MAP_TYPE_NG;
        } else if (varname == "surface_N") {
            mIspc.mAttributeMapType = ispc::PRIMVAR_MAP_TYPE_SURFACE_N;
            moonray::shading::TypedAttributeKey<Vec3f> attributeKey("surface_N");
            mIspc.mPrimitiveAttributeIndex = attributeKey;
            mOptionalAttributes.push_back(mIspc.mPrimitiveAttributeIndex);
        } else {
            // Not a special case so create key from the
            // "varname" value
            mIspc.mAttributeMapType = ispc::PRIMVAR_MAP_TYPE_PRIMITIVE_ATTRIBUTE;
            moonray::shading::TypedAttributeKey<Vec3f> attributeKey(varname);
            mIspc.mPrimitiveAttributeIndex = attributeKey;
            mOptionalAttributes.push_back(attributeKey);
        }
        ::moonshine::primvar::createLogEvent("normal", varname, mIspc.mMissingAttributeEvent, sLogEventRegistry);
    }

    mXform = std::make_unique<moonray::shading::Xform>(this, nullptr, nullptr, nullptr);
    mIspc.mXform = mXform->getIspcXform();
}

void
UsdPrimvarReader_normal::sampleNormal(const NormalMap *self,
                                      moonray::shading::TLState *tls,
                                      const moonray::shading::State &state,
                                      Vec3f *sample)
{
    const UsdPrimvarReader_normal* me = static_cast<const UsdPrimvarReader_normal*>(self);

    const int attributeMapType = me->mIspc.mAttributeMapType;
    if (attributeMapType == ispc::PRIMVAR_MAP_TYPE_N) {
        *sample = state.getN();
    } else if (attributeMapType == ispc::PRIMVAR_MAP_TYPE_NG) {
        *sample = state.getNg();
    } else {
        int key = me->mIspc.mPrimitiveAttributeIndex;
        if (state.isProvided(key)) {
            const Vec3f objectSpaceNormal = state.getAttribute(moonray::shading::TypedAttributeKey<Vec3f>(key));
            *sample = me->mXform->transformNormal(ispc::SHADING_SPACE_OBJECT,
                                                  ispc::SHADING_SPACE_RENDER,
                                                  state,
                                                  objectSpaceNormal);
        } else {
            // the primitive attribute is unavailable, use fallback parameter
            const Vec3f fallback = evalVec3f(me, attrFallback, tls, state);
            const ReferenceFrame frame(state.getN(), normalize(state.getdPds()));
            *sample = frame.localToGlobal(fallback);
            if (me->get(attrWarnWhenUnavailable)) {
                moonray::shading::logEvent(me, me->mIspc.mMissingAttributeEvent);
            }
        }
    }
}

