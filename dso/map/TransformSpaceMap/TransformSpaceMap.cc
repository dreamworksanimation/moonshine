// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.cc"
#include "TransformSpaceMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>

using namespace scene_rdl2::math;

//----------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(TransformSpaceMap, Map)

public:
    TransformSpaceMap(const SceneClass& sceneClass, const std::string& name);
    ~TransformSpaceMap() override;
    void update() override;

private:
    static void sample(const Map* self, moonray::shading::TLState* tls, const moonray::shading::State& state, Color* sample);
    ispc::TransformSpaceMap mIspc;
    std::unique_ptr<moonray::shading::Xform> mXform;

RDL2_DSO_CLASS_END(TransformSpaceMap)

//----------------------------------------------------------------------------

TransformSpaceMap::TransformSpaceMap(const SceneClass& sceneClass, const std::string& name)
    : Parent(sceneClass, name)
{
    mSampleFunc = TransformSpaceMap::sample;
    mSampleFuncv = (SampleFuncv) ispc::TransformSpaceMap_getSampleFunc();
    mIspc.mRefPKey = moonray::shading::StandardAttributes::sRefP;
    mIspc.mRefNKey = moonray::shading::StandardAttributes::sRefN;
    mIspc.mdPdsKey = moonray::shading::StandardAttributes::sdPds;
    mIspc.mRefdPdsKey = moonray::shading::StandardAttributes::sRefdPds;
    mIspc.mInstanceObjectTransformKey = moonray::shading::StandardAttributes::sInstanceObjectTransform;
    mIspc.mInstanceTransformKeys[0] = moonray::shading::StandardAttributes::sInstanceTransformLevel0;
    mIspc.mInstanceTransformKeys[1] = moonray::shading::StandardAttributes::sInstanceTransformLevel1;
    mIspc.mInstanceTransformKeys[2] = moonray::shading::StandardAttributes::sInstanceTransformLevel2;
    mIspc.mInstanceTransformKeys[3] = moonray::shading::StandardAttributes::sInstanceTransformLevel3;
    mIspc.mInstanceTransformKeys[4] = moonray::shading::StandardAttributes::sInstanceTransformLevel4;
}

TransformSpaceMap::~TransformSpaceMap()
{
}

void
TransformSpaceMap::update()
{
    // Construct Xform object with custom camera and custom square window
    std::array<float, 4> window = { -1.0f, -1.0f, 1.0f, 1.0f };
    if (get(attrUseCustomWindowCoordinates)) {
        window[0] = get(attrWindowXMin);
        window[1] = get(attrWindowYMin);
        window[2] = get(attrWindowXMax);
        window[3] = get(attrWindowYMax);
    }

    SceneObject const * obj = get(attrObject);
    Geometry const *rdlGeometry = obj ? obj->asA<Geometry>() : nullptr;

    SceneObject const * cam = get(attrCamera);
    Camera const *rdlCamera = cam ? cam->asA<Camera>() : nullptr;

    mXform = std::make_unique<moonray::shading::Xform>(this, rdlGeometry, rdlCamera, &window);
    mIspc.mXform = mXform->getIspcXform();

    // Use reference space
    if (hasChanged(attrFromSpace) || hasChanged(attrToSpace)) {
        mRequiredAttributes.clear();
        mOptionalAttributes.clear();
        if (get(attrToSpace) == ispc::TSM_TANGENT) {
            mRequiredAttributes.push_back(mIspc.mRefPKey);
            mOptionalAttributes.push_back(mIspc.mRefNKey);
            mOptionalAttributes.push_back(mIspc.mRefdPdsKey);
        } else if (get(attrFromSpace) == ispc::TSM_TANGENT) {
            mOptionalAttributes.push_back(mIspc.mdPdsKey);
        } else if (get(attrFromSpace) == ispc::TSM_INSTANCE_OBJECT || get(attrToSpace) == ispc::TSM_INSTANCE_OBJECT) {
            mOptionalAttributes.push_back(mIspc.mInstanceObjectTransformKey);
        } else if (get(attrFromSpace) >= ispc::TSM_INSTANCE_OFFSET || get(attrToSpace) >= ispc::TSM_INSTANCE_OFFSET) {
            for (int i = 0; i < 5; ++i) {
                mOptionalAttributes.push_back(mIspc.mInstanceTransformKeys[i]);
            }
        }
    }
}

namespace {

Vec3f
transformByInstanceObject(bool invert,
                          ispc::InputType inputType,
                          const moonray::shading::State& state,
                          const Vec3f& inputVal)
{
    if (!state.isProvided(moonray::shading::StandardAttributes::sInstanceObjectTransform)) {
        return inputVal;
    }

    Mat4f instanceObjectTransform =
        state.getAttribute(moonray::shading::StandardAttributes::sInstanceObjectTransform);

    if (invert) {
        instanceObjectTransform = instanceObjectTransform.inverse();
    }

    switch (inputType) {
    case ispc::INPUT_TYPE_POINT:
        return transformPoint(instanceObjectTransform,
                                    inputVal);
        break;
    case ispc::INPUT_TYPE_VECTOR:
        return transformVector(instanceObjectTransform,
                                     inputVal);
        break;
    case ispc::INPUT_TYPE_NORMAL:
        instanceObjectTransform = instanceObjectTransform.inverse();
        return transformNormal(instanceObjectTransform,
                                     inputVal);
        break;
    default :
        return inputVal;
    }
}

Vec3f
transformByInstance(const ispc::TransformSpaceMap& mIspc,
                    int instanceLevel,
                    bool invert,
                    ispc::InputType inputType,
                    const moonray::shading::State& state,
                    const Vec3f& inputVal,
                    bool concatenateTransforms)
{
    Mat4f instanceTransformResult(1.0f);

    if (invert) {
        const int startIndex = (concatenateTransforms) ? 4 : instanceLevel;
        for (int i = startIndex; i >= instanceLevel; --i) {

            moonray::shading::TypedAttributeKey<Mat4f> instanceTransformLevelKey =
                static_cast<moonray::shading::TypedAttributeKey<Mat4f>>(mIspc.mInstanceTransformKeys[i]);

            if (state.isProvided(instanceTransformLevelKey)) {

                Mat4f instanceTransform = state.getAttribute(
                    static_cast<moonray::shading::TypedAttributeKey<Mat4f>>(instanceTransformLevelKey));

                instanceTransform = instanceTransform.inverse();
                if (concatenateTransforms) {
                    instanceTransformResult *= instanceTransform;
                } else {
                    instanceTransformResult = instanceTransform;
                }
            }
        }
    } else {
        const int startIndex = (concatenateTransforms) ? 0 : instanceLevel;
        for (int i = startIndex; i <= instanceLevel; ++i) {

            moonray::shading::TypedAttributeKey<Mat4f> instanceTransformLevelKey =
                static_cast<moonray::shading::TypedAttributeKey<Mat4f>>(mIspc.mInstanceTransformKeys[i]);

            if (state.isProvided(instanceTransformLevelKey)) {

                Mat4f instanceTransform = state.getAttribute(
                    static_cast<moonray::shading::TypedAttributeKey<Mat4f>>(instanceTransformLevelKey));

                if (concatenateTransforms) {
                    instanceTransformResult *= instanceTransform;
                } else {
                    instanceTransformResult = instanceTransform;
                }
            }
        }
    }

    switch (inputType) {
    case ispc::INPUT_TYPE_POINT:
        return transformPoint(instanceTransformResult,
                                    inputVal);
        break;
    case ispc::INPUT_TYPE_VECTOR:
        return transformVector(instanceTransformResult,
                                     inputVal);
        break;
    case ispc::INPUT_TYPE_NORMAL:
        instanceTransformResult = instanceTransformResult.inverse();
        return transformNormal(instanceTransformResult,
                                     inputVal);
        break;
    default :
        return inputVal;
    }
}

Vec3f
transformType(const moonray::shading::Xform& mXform,
              int fromSpace,
              int toSpace,
              ispc::InputType inputType,
              const moonray::shading::State& state,
              const Vec3f& inputVal)
{
    switch(inputType) {
    case ispc::INPUT_TYPE_POINT:
        return mXform.transformPoint(fromSpace,
                                     toSpace,
                                     state,
                                     inputVal);
        break;
    case ispc::INPUT_TYPE_VECTOR:
        return mXform.transformVector(fromSpace,
                                      toSpace,
                                      state,
                                      inputVal);
        break;
    case ispc::INPUT_TYPE_NORMAL:
        return mXform.transformNormal(fromSpace,
                                      toSpace,
                                      state,
                                      inputVal);
        break;
    default :
        return inputVal;
    }
}

Vec3f
localToRender(const moonray::shading::State& state,
              const int refNKey,
              const int dPdsKey,
              const Vec3f& inputVal)
{
    const Vec3f curN = state.isProvided(refNKey) ?
                       state.getN() :
                       state.getNg();

    Vec3f dPds;
    if (state.isProvided(dPdsKey)) {
        dPds = state.getAttribute(static_cast<moonray::shading::TypedAttributeKey<Vec3f>>(dPdsKey));
    }  else {
        dPds = normalize(state.getdPds());
    }

    const ReferenceFrame frame(normalize(curN), dPds);

    // From local tangent space to render space
    return frame.localToGlobal(inputVal);
}

Vec3f
worldToRefTangent(const moonray::shading::State& state,
                  const int refPKey,
                  const int refNKey,
                  const int refdPdsKey,
                  const Vec3f& inputVal)
{
    Vec3f dRefPds;
    if (state.isProvided(refdPdsKey)) {
       dRefPds = state.getAttribute(static_cast<moonray::shading::TypedAttributeKey<Vec3f>>(refdPdsKey));
    } else {
        state.getdVec3fAttrds(refPKey, dRefPds);
    }
    dRefPds = normalize(dRefPds);

    Vec3f refN;
    if (state.isProvided(refNKey)) {
        refN = state.getAttribute(
            static_cast<moonray::shading::TypedAttributeKey<Vec3f>>(refNKey));
    } else {
        Vec3f dRefPdt;
        state.getdVec3fAttrdt(refPKey, dRefPdt);
        dRefPdt = normalize(dRefPdt);
        refN = cross(dRefPds, dRefPdt);
    }

    const ReferenceFrame refFrame(normalize(refN), dRefPds);

    // From world space to local reference tangent space
    return refFrame.globalToLocal(inputVal);
}

} // end anonymous namespace


void
TransformSpaceMap::sample(const Map* self,
                          moonray::shading::TLState* tls,
                          const moonray::shading::State& state,
                          Color* sample)
{
    const TransformSpaceMap* me = static_cast<const TransformSpaceMap*>(self);

    const Vec3f inputVal = evalVec3f(me, attrInput, tls, state);
    const ispc::InputType inputType = static_cast<ispc::InputType>(me->get(attrInputType));
    const int fromSpace = me->get(attrFromSpace);
    const int toSpace = me->get(attrToSpace);
    const bool concatenateInstanceLevelTransforms = me->get(attrConcatenateTransforms);

    Vec3f result = inputVal;

    // If not using the tangent space or instance options, things are fairly simple
    if (fromSpace < ispc::TSM_TANGENT && toSpace < ispc::TSM_TANGENT) {
        // If neither space is tangent/reference tangent then
        // we transform directly from "fromSpace" to "toSpace"

        result = transformType(*(me->mXform.get()),
                               fromSpace,
                               toSpace,
                               inputType,
                               state,
                               inputVal);

        *sample = Color(result[0],
                              result[1],
                              result[2]);

        // early exit, we're done
        return;
    }
    
    // handle special simple case
    if (fromSpace == ispc::TSM_TANGENT &&
        toSpace == ispc::SHADING_SPACE_RENDER) {

        result = localToRender(state,
                               me->mIspc.mRefNKey,
                               me->mIspc.mdPdsKey,
                               inputVal);
        *sample = Color(result[0],
                              result[1],
                              result[2]);

        // early exit, we're done
        return;
    }    

    // If either or both spaces are "local tangent" or 
    // "local reference tangent" then we transform first
    // from "fromSpace" to  world space and then from world
    // space to "toSpace"
    Vec3f worldSpaceVal;
    if (fromSpace == ispc::TSM_TANGENT) {
        Vec3f renderSpaceVal = localToRender(state,
                                             me->mIspc.mRefNKey,
                                             me->mIspc.mdPdsKey,
                                             inputVal);

        // convert render space value to world space
        worldSpaceVal = transformType(*(me->mXform.get()),
                                      ispc::SHADING_SPACE_RENDER,
                                      ispc::SHADING_SPACE_WORLD,
                                      inputType,
                                      state,
                                      renderSpaceVal);
    } else if (fromSpace == ispc::TSM_INSTANCE_OBJECT) {
        worldSpaceVal = transformByInstanceObject(false, // invert
                                                  inputType,
                                                  state,
                                                  inputVal);
    } else if (fromSpace == ispc::SHADING_SPACE_WORLD) {
        worldSpaceVal = inputVal;
    } else if (fromSpace >= ispc::TSM_INSTANCE_OFFSET) {
        const int instanceLevel = fromSpace - ispc::TSM_INSTANCE_OFFSET;
        worldSpaceVal = transformByInstance(me->mIspc,
                                            instanceLevel,
                                            false, // invert
                                            inputType,
                                            state,
                                            inputVal,
                                            concatenateInstanceLevelTransforms);
    } else {
        worldSpaceVal = transformType(*(me->mXform.get()),
                                      fromSpace,
                                      ispc::SHADING_SPACE_WORLD,
                                      inputType,
                                      state,
                                      inputVal);
    }

    // Transform from world space to "to_space"
    if (toSpace == ispc::TSM_TANGENT) {
        result = worldToRefTangent(state,
                                   me->mIspc.mRefPKey,
                                   me->mIspc.mRefNKey,
                                   me->mIspc.mRefdPdsKey,
                                   worldSpaceVal);
    } else if (toSpace == ispc::TSM_INSTANCE_OBJECT) {
        result = transformByInstanceObject(true, // invert
                                           inputType,
                                           state,
                                           inputVal);
    } else if (toSpace == ispc::SHADING_SPACE_WORLD) {
        result = worldSpaceVal;
    } else if (toSpace >= ispc::TSM_INSTANCE_OFFSET) {
        const int instanceLevel = toSpace - ispc::TSM_INSTANCE_OFFSET;
        result = transformByInstance(me->mIspc,
                                     instanceLevel,
                                     true, // invert
                                     inputType,
                                     state,
                                     worldSpaceVal,
                                     concatenateInstanceLevelTransforms);
    } else {
        result = transformType(*(me->mXform.get()),
                               ispc::SHADING_SPACE_WORLD,
                               toSpace,
                               inputType,
                               state,
                               worldSpaceVal);
    }

    *sample = Color(result[0],
                    result[1],
                    result[2]);
}

