// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

struct AxisAngleMap
{
    const uniform Xform * uniform mXform;
};

ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(AxisAngleMap);

static Color
sample(const uniform Map* uniform map, uniform ShadingTLState* uniform tls, const varying State& state)
{
    const uniform AxisAngleMap * uniform me = MAP_GET_ISPC_PTR(AxisAngleMap, map);

    varying Vec3f inputVector = evalAttrInputVector(map, tls, state);
    const uniform int inputSpace = getAttrInputSpace(map);
    const uniform int axisSpace = getAttrAxisSpace(map);
    const uniform int outputSpace = getAttrOutputSpace(map);
    const varying float angle = deg2rad(evalAttrAngle(map, tls, state));

    inputVector = transformNormal(me->mXform,
                                 inputSpace,
                                 axisSpace,
                                 state,
                                 inputVector);

    const varying Vec3f rotationAxis = evalAttrRotationAxis(map, tls, state);
    const varying Mat4f mat = Mat4f_rotate(rotationAxis, angle);
    varying Vec3f outputVector = transformVector(mat, inputVector);

    outputVector = transformNormal(me->mXform,
                                   axisSpace,
                                   outputSpace,
                                   state,
                                   outputVector);

    return asAColor(outputVector);
}

DEFINE_MAP_SHADER(AxisAngleMap, sample)

