// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

struct TransformSpaceMap
{
    const uniform Xform * uniform mXform;
    uniform int mRefPKey;
    uniform int mRefNKey;
    uniform int mInstanceObjectTransformKey;
    uniform int mInstanceTransformKeys[5];
};
ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(TransformSpaceMap);

// keep in sync with .json enum
enum TsmSpaces {
    TSM_RENDER,
    TSM_CAMERA,
    TSM_WORLD,
    TSM_SCREEN,
    TSM_OBJECT,
    TSM_TANGENT,
    TSM_INSTANCE_OBJECT,
    TSM_INSTANCE_OFFSET,
    TSM_INSTANCE_0,
    TSM_INSTANCE_1,
    TSM_INSTANCE_2,
    TSM_INSTANCE_3,
    TSM_INSTANCE_4,
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(TsmSpaces);

enum InputType {
    INPUT_TYPE_POINT,
    INPUT_TYPE_VECTOR,
    INPUT_TYPE_NORMAL
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(InputType);

Vec3f
transformByInstanceObject(const uniform TransformSpaceMap* uniform me,
                          const uniform bool invert,
                          const uniform InputType inputType,
                          uniform ShadingTLState* uniform tls,
                          const varying State& state,
                          const varying Vec3f& inputVal)
{
    const uniform int instanceObjectTransformKey = me->mInstanceObjectTransformKey;
    if (!isProvided(state, instanceObjectTransformKey)) {
        return inputVal;
    }

    Mat4f instanceObjectTransform = getMat4fAttribute(tls, state, instanceObjectTransformKey);
    if (invert) {
        instanceObjectTransform = inverse(instanceObjectTransform);
    }

    switch (inputType) {
    case INPUT_TYPE_POINT:
        return transformPoint(instanceObjectTransform,
                              inputVal);
        break;
    case INPUT_TYPE_VECTOR:
        return transformVector(instanceObjectTransform,
                               inputVal);
        break;
    case INPUT_TYPE_NORMAL:
        instanceObjectTransform = inverse(instanceObjectTransform);
        return transformNormal(instanceObjectTransform,
                               inputVal);
        break;
    default :
        return inputVal;
    }
}

Vec3f
transformByInstance(const uniform TransformSpaceMap* uniform me,
                    const uniform int instanceLevel,
                    const uniform bool invert,
                    const uniform InputType inputType,
                    uniform ShadingTLState* uniform tls,
                    const varying State& state,
                    const varying Vec3f& inputVal,
                    const uniform bool concatenateTransforms)
{
    Mat4f instanceTransformResult;
    setToIdentity(instanceTransformResult);

    if (invert) {
        const uniform int startIndex = (concatenateTransforms) ? 4 : instanceLevel;
        for (uniform int i = startIndex; i >= instanceLevel; --i) {

            const uniform int instanceTransformLevelKey = me->mInstanceTransformKeys[i];

            if (isProvided(state, instanceTransformLevelKey)) {

                Mat4f instanceTransform = getMat4fAttribute(tls,
                                                            state,
                                                            instanceTransformLevelKey);

                instanceTransform = inverse(instanceTransform);
                if (concatenateTransforms) {
                    instanceTransformResult = instanceTransformResult * instanceTransform;
                } else {
                    instanceTransformResult = instanceTransform;
                }
            }
        }
    } else {
        const uniform int startIndex = (concatenateTransforms) ? 0 : instanceLevel;
        for (uniform int i = startIndex; i <= instanceLevel; ++i) {

            const uniform int instanceTransformLevelKey = me->mInstanceTransformKeys[i];

            if (isProvided(state, instanceTransformLevelKey)) {

                Mat4f instanceTransform = getMat4fAttribute(tls,
                                                            state,
                                                            instanceTransformLevelKey);
                if (concatenateTransforms) {
                    instanceTransformResult = instanceTransformResult * instanceTransform;
                } else {
                    instanceTransformResult = instanceTransform;
                }
            }
        }
    }

    switch (inputType) {
    case INPUT_TYPE_POINT:
        return transformPoint(instanceTransformResult,
                              inputVal);
        break;
    case INPUT_TYPE_VECTOR:
        return transformVector(instanceTransformResult,
                               inputVal);
        break;
    case INPUT_TYPE_NORMAL:
        instanceTransformResult = inverse(instanceTransformResult);
        return transformNormal(instanceTransformResult,
                               inputVal);
        break;
    default :
        return inputVal;
    }
}

Vec3f
transformType(const uniform Xform * uniform xform,
              const uniform int fromSpace,
              const uniform int toSpace,
              const uniform InputType inputType,
              const varying State& state,
              const varying Vec3f inputVal)
{
    switch(inputType) {
    case INPUT_TYPE_POINT:
        return transformPoint(xform,
                              fromSpace,
                              toSpace,
                              state,
                              inputVal);
        break;
    case INPUT_TYPE_VECTOR:
        return transformVector(xform,
                               fromSpace,
                               toSpace,
                               state,
                               inputVal);
        break;
    case INPUT_TYPE_NORMAL:
        return transformNormal(xform,
                               fromSpace,
                               toSpace,
                               state,
                               inputVal);
        break;
    default :
        return inputVal;
    }
}

Vec3f
localToRender(const varying State &state,
              const uniform int refNKey,
              const varying Vec3f& inputVal)
{
    Vec3f curN;
    if (isProvided(state, refNKey)) {
        curN = getN(state);
    } else {
        curN = getNg(asAnIntersection(state));
    }

    ReferenceFrame frame;
    ReferenceFrame_init(frame, normalize(curN), normalize(getdPds(state)));

    // From local tangent space to render space
    Vec3f tmp = localToGlobal(frame, inputVal);
    return tmp;
}

Vec3f
worldToRefTangent(const varying State &state,
                  const uniform int refPKey,
                  const uniform int refNKey,
                  uniform ShadingTLState * uniform tls,
                  const varying Vec3f& inputVal)
{
    Vec3f dRefPds = normalize(getdVec3fAttributeds(tls, state, refPKey));

    Vec3f refN;
    if (isProvided(state, refNKey)) {
        SHADER_DATA_getRefN(refPKey, refNKey, tls, state, &refN);
    } else {
        Vec3f dRefPdt = normalize(getdVec3fAttributedt(tls, state, refPKey));
        refN = cross(dRefPds, dRefPdt);
    }

    ReferenceFrame refFrame;
    ReferenceFrame_init(refFrame, normalize(refN), dRefPds);

    // From world space to local reference tangent space
    Vec3f tmp = globalToLocal(refFrame, inputVal);
    return tmp;
}

static Color
sample(const uniform Map* uniform map,
       uniform ShadingTLState* uniform tls,
       const varying State& state)
{
    const uniform TransformSpaceMap* uniform me =
        MAP_GET_ISPC_PTR(TransformSpaceMap, map);

    const varying Vec3f inputVal = evalAttrInput(map, tls, state);
    const uniform InputType inputType = (InputType)getAttrInputType(map);
    const uniform int fromSpace = getAttrFromSpace(map);
    const uniform int toSpace = getAttrToSpace(map);
    const uniform bool concatenateInstanceLevelTransforms = getAttrConcatenateTransforms(map);

    varying Vec3f result;

    // If not using the tangent space or instance options, things are fairly simple
    if (fromSpace < TSM_TANGENT && toSpace < TSM_TANGENT) {
        // If neither space is tangent/reference tangent then
        // we transform directly from "fromSpace" to "toSpace"

        result = transformType(me->mXform,
                               fromSpace,
                               toSpace,
                               inputType,
                               state,
                               inputVal);

        // early exit, we're done
        return asAColor(result);
    }

    // handle special simple case
    if (fromSpace == TSM_TANGENT &&
        toSpace == SHADING_SPACE_RENDER) {

        result = localToRender(state,
                               me->mRefNKey,
                               inputVal);

        // early exit, we're done
        return asAColor(result);
    }    

    // If either or both spaces are "local tangent" or 
    // "local reference tangent" then we transform first
    // from "fromSpace" to  world space and then from world
    // space to "toSpace"

    varying Vec3f worldSpaceVal;
    if (fromSpace == TSM_TANGENT) {
        Vec3f renderSpaceVal = localToRender(state,
                                             me->mRefNKey,
                                             inputVal);

        // convert render space value to world space
        worldSpaceVal = transformType(me->mXform,
                                      SHADING_SPACE_RENDER,
                                      SHADING_SPACE_WORLD,
                                      inputType,
                                      state,
                                      renderSpaceVal);
    } else if (fromSpace == TSM_INSTANCE_OBJECT) {
        worldSpaceVal = transformByInstanceObject(me,
                                                  false, // invert
                                                  inputType,
                                                  tls,
                                                  state,
                                                  inputVal);
    } else if (fromSpace == SHADING_SPACE_WORLD) {
        worldSpaceVal = inputVal;
    } else if (fromSpace >= TSM_INSTANCE_OFFSET) {
        const uniform int instanceLevel = fromSpace - TSM_INSTANCE_OFFSET;
        worldSpaceVal = transformByInstance(me,
                                            instanceLevel,
                                            false, // invert
                                            inputType,
                                            tls,
                                            state,
                                            inputVal,
                                            concatenateInstanceLevelTransforms);
    } else {
        worldSpaceVal = transformType(me->mXform,
                                      fromSpace,
                                      SHADING_SPACE_WORLD,
                                      inputType,
                                      state,
                                      inputVal);
    }

    // Transform from world space to "to_space"
    if (toSpace == TSM_TANGENT) {
        result = worldToRefTangent(state,
                                   me->mRefPKey,
                                   me->mRefNKey,
                                   tls,
                                   worldSpaceVal);
    } else if (toSpace == TSM_INSTANCE_OBJECT) {
        result = transformByInstanceObject(me,
                                           true, // invert
                                           inputType,
                                           tls,
                                           state,
                                           inputVal);
    } else if (toSpace == SHADING_SPACE_WORLD) {
        result = worldSpaceVal;
    } else if (toSpace >= TSM_INSTANCE_OFFSET) {
        const uniform int instanceLevel = toSpace - TSM_INSTANCE_OFFSET;
        result = transformByInstance(me,
                                     instanceLevel,
                                     true, // invert
                                     inputType,
                                     tls,
                                     state,
                                     worldSpaceVal,
                                     concatenateInstanceLevelTransforms);
    } else {
        result = transformType(me->mXform,
                               SHADING_SPACE_WORLD,
                               toSpace,
                               inputType,
                               state,
                               worldSpaceVal);
    }

    return asAColor(result);
}

DEFINE_MAP_SHADER(TransformSpaceMap, sample)

