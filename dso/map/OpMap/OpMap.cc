// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.cc"
#include "OpMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>

#include <random>
#include <string>
#include <vector>

using namespace scene_rdl2::math;
using namespace moonray::shading;

namespace {

Color
colorDivide(const Color &a, const Color &b)
{
    const Color denom = scene_rdl2::math::max(b, Color(0.000001, 0.000001, 0.000001));
    return a / denom;
}

Color
colorCross(const Color &a, const Color &b)
{
    const Vec3f vecA = Vec3f(a[0], a[1], a[2]);
    const Vec3f vecB = Vec3f(b[0], b[1], b[2]);
    const Vec3f vecC = scene_rdl2::math::cross(vecA, vecB);
    return Color(vecC[0], vecC[1], vecC[2]);
}

Color
colorDot(const Color &a, const Color &b)
{
    const Vec3f vecA = Vec3f(a[0], a[1], a[2]);
    const Vec3f vecB = Vec3f(b[0], b[1], b[2]);
    const float dotResult = scene_rdl2::math::dot(vecA, vecB);
    return Color(dotResult, dotResult, dotResult);
}

Color
colorNormalize(const Color &a)
{
    Vec3f vecA = Vec3f(a[0], a[1], a[2]);
    vecA.safeNormalize();
    return Color(vecA[0], vecA[1], vecA[2]);
}

Color
colorOverlay(const Color &a, const Color &b)
{
    Color result;
    if (b[0] < 0.5) result[0] = scene_rdl2::math::clamp(a[0] * b[0] * 2.0, 0.0, 1.0);
    else result[0] = scene_rdl2::math::clamp(1.0 -  (1.0 - a[0]) * (1.0 - b[0]) * 2.0, 0.0, 1.0);
    if (b[1] < 0.5) result[1] = scene_rdl2::math::clamp(a[1] * b[1] * 2.0, 0.0, 1.0);
    else result[1] = scene_rdl2::math::clamp(1.0 -  (1.0 - a[1]) * (1.0 - b[1]) * 2.0, 0.0, 1.0);
    if (b[2] < 0.5) result[2] = scene_rdl2::math::clamp(a[2] * b[2] * 2.0, 0.0, 1.0);
    else result[2] = scene_rdl2::math::clamp(1.0 -  (1.0 - a[2]) * (1.0 - b[2]) * 2.0, 0.0, 1.0);
    return result;
}

Color
colorScreen(const Color &a, const Color &b)
{
    Color result;
    result[0] = scene_rdl2::math::clamp(1.0 - (1.0 - a[0]) * (1.0 - b[0]), 0.0, 1.0);
    result[1] = scene_rdl2::math::clamp(1.0 - (1.0 - a[1]) * (1.0 - b[1]), 0.0, 1.0);
    result[2] = scene_rdl2::math::clamp(1.0 - (1.0 - a[2]) * (1.0 - b[2]), 0.0, 1.0);
    return result;
}

Color
colorAbs(const Color &a)
{
	Color result;
	result[0] = scene_rdl2::math::abs(a[0]);
	result[1] = scene_rdl2::math::abs(a[1]);
	result[2] = scene_rdl2::math::abs(a[2]);
	return result;
}

Color
colorCeil(const Color &a)
{
	Color result;
	result[0] = scene_rdl2::math::ceil(a[0]);
	result[1] = scene_rdl2::math::ceil(a[1]);
	result[2] = scene_rdl2::math::ceil(a[2]);
	return result;
}

Color
colorFloor(const Color &a)
{
	Color result;
	result[0] = scene_rdl2::math::floor(a[0]);
	result[1] = scene_rdl2::math::floor(a[1]);
	result[2] = scene_rdl2::math::floor(a[2]);
	return result;
}

Color
colorModulo(const Color &a, const Color &b)
{
	Color result;
	result[0] = scene_rdl2::math::fmod(a[0], b[0]);
	result[1] = scene_rdl2::math::fmod(a[1], b[1]);
	result[2] = scene_rdl2::math::fmod(a[2], b[2]);
	return result;
}

Color
colorFrac(const Color &a)
{
	Color result;
	result[0] = a[0] - scene_rdl2::math::floor(a[0]);
	result[1] = a[1] - scene_rdl2::math::floor(a[1]);
	result[2] = a[2] - scene_rdl2::math::floor(a[2]);
	return result;
}

Color
colorLength(const Color &a)
{
	Vec3f vecA = Vec3f(a[0], a[1], a[2]);
	float length = vecA.length();
	return Color(length);
}

Color
colorSin(const Color &a)
{
   Color result;
   result[0] = scene_rdl2::math::sin(a[0]);
   result[1] = scene_rdl2::math::sin(a[1]);
   result[2] = scene_rdl2::math::sin(a[2]);
   return result;
}

Color
colorCos(const Color &a)
{
   Color result;
   result[0] = scene_rdl2::math::cos(a[0]);
   result[1] = scene_rdl2::math::cos(a[1]);
   result[2] = scene_rdl2::math::cos(a[2]);
   return result;
}

Color
colorAcos(const Color &a)
{
   Color result;
   result[0] = scene_rdl2::math::acos(scene_rdl2::math::clamp(a[0], -1.0f, 1.0f));
   result[1] = scene_rdl2::math::acos(scene_rdl2::math::clamp(a[1], -1.0f, 1.0f));
   result[2] = scene_rdl2::math::acos(scene_rdl2::math::clamp(a[2], -1.0f, 1.0f));
   return result;
}

float
floatAlmostEquals(const float a, const float b, const float epsilon)
{
    return std::abs(a - b) <= epsilon ? 1.0f : 0.0f;
}

}

//---------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(OpMap, scene_rdl2::rdl2::Map)

public:
    OpMap(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name);
    ~OpMap();
    virtual void update();

private:
    static void sample(const scene_rdl2::rdl2::Map* self, moonray::shading::TLState *tls,
                       const moonray::shading::State& state, Color* sample);

RDL2_DSO_CLASS_END(OpMap)

//---------------------------------------------------------------------------

OpMap::OpMap(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mSampleFunc = OpMap::sample;
    mSampleFuncv = (scene_rdl2::rdl2::SampleFuncv) ispc::OpMap_getSampleFunc();
}

OpMap::~OpMap()
{
}

void
OpMap::update()
{
    const int operation = get(attrOperation);
    if (operation < 0 || operation > 40) {
        fatal("Unsupported operation mode");
    }
}

void
OpMap::sample(const scene_rdl2::rdl2::Map* self, moonray::shading::TLState *tls,
                 const moonray::shading::State& state, Color* sample)
{
    const OpMap* me = static_cast<const OpMap*>(self);

    // Get the input parameters
    const int   operation = me->get(attrOperation);
    const Color op1 = evalColor(me, attrOp1, tls, state);
    const Color op2 = evalColor(me, attrOp2, tls, state);
    const float op1Factor = evalFloat(me, attrOp1Factor, tls, state);
    const float op2Factor = evalFloat(me, attrOp2Factor, tls, state);
    const bool  doClamp   = me->get(attrClamp);

    const Color opOne = op1 * op1Factor;
    const Color opTwo = op2 * op2Factor;

    const float tolerance = me->get(attrTolerance);

    Color result(op1);

    switch(operation) {
        case ispc::OpMapType::ADD:
            result = opOne + opTwo;
            break;
        case ispc::OpMapType::SUBTRACT:
            result = opOne - opTwo;
            break;
        case ispc::OpMapType::MULTIPLY:
            result = opOne * opTwo;
            break;
        case ispc::OpMapType::DIVIDE:
            result = colorDivide(opOne, opTwo);
            break;
        case ispc::OpMapType::INVERT:
            result[0] = 1.0f - opOne[0];
            result[1] = 1.0f - opOne[1];
            result[2] = 1.0f - opOne[2];
            break;
        case ispc::OpMapType::MAXIMUM:
            result = max(opOne, opTwo);
            break;
        case ispc::OpMapType::MINIMUM:
            result = min(opOne, opTwo);
            break;
        case ispc::OpMapType::POWER:
            {
                Color base = max(opOne, Color(sEpsilon));
                result[0] = pow(base[0], opTwo[0]);
                result[1] = pow(base[1], opTwo[1]);
                result[2] = pow(base[2], opTwo[2]);
                break;
            }
        case ispc::OpMapType::CROSS:
            result = colorCross(opOne, opTwo);
            break;
        case ispc::OpMapType::DOT:
            result = colorDot(opOne, opTwo);
            break;
        case ispc::OpMapType::NORMALIZE:
            result = colorNormalize(opOne);
            break;
        case ispc::OpMapType::OP1:
            result = opOne;
            break;
        case ispc::OpMapType::OP2:
            result = opTwo;
            break;
        case ispc::OpMapType::OVERLAY:
            result = colorOverlay(opOne, opTwo);
            break;
        case ispc::OpMapType::SCRN:
            result = colorScreen(opOne, opTwo);
            break;
        case ispc::OpMapType::ABS:
        	result = colorAbs(opOne);
            break;
        case ispc::OpMapType::CEIL:
        	result = colorCeil(opOne);
            break;
        case ispc::OpMapType::FLOOR:
        	result = colorFloor(opOne);
            break;
        case ispc::OpMapType::MODULO:
        	result = colorModulo(opOne, opTwo);
        	break;
        case ispc::OpMapType::FRACTION:
        	result = colorFrac(opOne);
        	break;
        case ispc::OpMapType::LENGTH:
        	result = colorLength(opOne);
        	break;
        case ispc::OpMapType::SINE:
        	result = colorSin(opOne);
        	break;
        case ispc::OpMapType::COSINE:
        	result = colorCos(opOne);
        	break;
        case ispc::OpMapType::ROUND:
            result[0] = static_cast<float>(static_cast<int>(opOne[0] < 0.0f ? opOne[0] - 0.5f : opOne[0] + 0.5f));
            result[1] = static_cast<float>(static_cast<int>(opOne[1] < 0.0f ? opOne[1] - 0.5f : opOne[1] + 0.5f));
            result[2] = static_cast<float>(static_cast<int>(opOne[2] < 0.0f ? opOne[2] - 0.5f : opOne[2] + 0.5f));
        	break;
        case ispc::OpMapType::ACOS:
        	result = colorAcos(opOne);
        	break;
        case ispc::OpMapType::LESS_THAN:
        	result[0] = static_cast<float>(opOne[0] < opTwo[0]);
        	result[1] = static_cast<float>(opOne[1] < opTwo[1]);
        	result[2] = static_cast<float>(opOne[2] < opTwo[2]);
        	break;
        case ispc::OpMapType::LESS_THAN_OR_EQUAL:
        	result[0] = static_cast<float>(opOne[0] <= opTwo[0]);
        	result[1] = static_cast<float>(opOne[1] <= opTwo[1]);
        	result[2] = static_cast<float>(opOne[2] <= opTwo[2]);
        	break;
        case ispc::OpMapType::GREATER_THAN:
        	result[0] = static_cast<float>(opOne[0] > opTwo[0]);
        	result[1] = static_cast<float>(opOne[1] > opTwo[1]);
        	result[2] = static_cast<float>(opOne[2] > opTwo[2]);
        	break;
        case ispc::OpMapType::GREATER_THAN_OR_EQUAL:
        	result[0] = static_cast<float>(opOne[0] >= opTwo[0]);
        	result[1] = static_cast<float>(opOne[1] >= opTwo[1]);
        	result[2] = static_cast<float>(opOne[2] >= opTwo[2]);
        	break;
        case ispc::OpMapType::EQUAL:
        	result[0] = floatAlmostEquals(opOne[0], opTwo[0], tolerance);
        	result[1] = floatAlmostEquals(opOne[1], opTwo[1], tolerance);
        	result[2] = floatAlmostEquals(opOne[2], opTwo[2], tolerance);
        	break;
        case ispc::OpMapType::NOT_EQUAL:
        	result[0] = 1.0f - floatAlmostEquals(opOne[0], opTwo[0], tolerance);
        	result[1] = 1.0f - floatAlmostEquals(opOne[1], opTwo[1], tolerance);
        	result[2] = 1.0f - floatAlmostEquals(opOne[2], opTwo[2], tolerance);
        	break;
        case ispc::OpMapType::AND:
        	result[0] = static_cast<float>(opOne[0] && opTwo[0]);
        	result[1] = static_cast<float>(opOne[1] && opTwo[1]);
        	result[2] = static_cast<float>(opOne[2] && opTwo[2]);
        	break;
        case ispc::OpMapType::OR:
        	result[0] = static_cast<float>(opOne[0] || opTwo[0]);
        	result[1] = static_cast<float>(opOne[1] || opTwo[1]);
        	result[2] = static_cast<float>(opOne[2] || opTwo[2]);
        	break;
        case ispc::OpMapType::NOT:
        	result[0] = static_cast<float>(!opOne[0]);
        	result[1] = static_cast<float>(!opOne[1]);
        	result[2] = static_cast<float>(!opOne[2]);
        	break;
        case ispc::OpMapType::XOR:
        	result[0] = static_cast<float>(static_cast<int>(opOne[0]) ^ static_cast<int>(opTwo[0]));
        	result[1] = static_cast<float>(static_cast<int>(opOne[1]) ^ static_cast<int>(opTwo[1]));
        	result[2] = static_cast<float>(static_cast<int>(opOne[2]) ^ static_cast<int>(opTwo[2]));
        	break;
        case ispc::OpMapType::BIT_SHIFT_LEFT:
        	result[0] = static_cast<float>(static_cast<int>(opOne[0]) << static_cast<int>(opTwo[0]));
        	result[1] = static_cast<float>(static_cast<int>(opOne[1]) << static_cast<int>(opTwo[1]));
        	result[2] = static_cast<float>(static_cast<int>(opOne[2]) << static_cast<int>(opTwo[2]));
        	break;
        case ispc::OpMapType::BIT_SHIFT_RIGHT:
        	result[0] = static_cast<float>(static_cast<int>(opOne[0]) >> static_cast<int>(opTwo[0]));
        	result[1] = static_cast<float>(static_cast<int>(opOne[1]) >> static_cast<int>(opTwo[1]));
        	result[2] = static_cast<float>(static_cast<int>(opOne[2]) >> static_cast<int>(opTwo[2]));
        	break;
        case ispc::OpMapType::BITWISE_AND:
        	result[0] = static_cast<float>(static_cast<int>(opOne[0]) & static_cast<int>(opTwo[0]));
        	result[1] = static_cast<float>(static_cast<int>(opOne[1]) & static_cast<int>(opTwo[1]));
        	result[2] = static_cast<float>(static_cast<int>(opOne[2]) & static_cast<int>(opTwo[2]));
        	break;
        case ispc::OpMapType::BITWISE_OR:
        	result[0] = static_cast<float>(static_cast<int>(opOne[0]) | static_cast<int>(opTwo[0]));
        	result[1] = static_cast<float>(static_cast<int>(opOne[1]) | static_cast<int>(opTwo[1]));
        	result[2] = static_cast<float>(static_cast<int>(opOne[2]) | static_cast<int>(opTwo[2]));
        	break;
        case ispc::OpMapType::VECTOR_EQUAL:
            {
                float distance = colorLength(opOne - opTwo)[0];
                result = Color(floatAlmostEquals(distance, 0.0f, tolerance));
                break;
            }
        case ispc::OpMapType::VECTOR_NOT_EQUAL:
            {
                float distance = colorLength(opOne - opTwo)[0];
                result = Color(1.0f - floatAlmostEquals(distance, 0.0f, tolerance));
                break;
            }
        default:
            result[0] = 0.0;
            result[1] = 0.0;
            result[2] = 0.0;
            break;
    }

    if (doClamp) {
        result[0] = clamp(result[0], 0.f, 1.f);
        result[1] = clamp(result[1], 0.f, 1.f);
        result[2] = clamp(result[2], 0.f, 1.f);
    }

    *sample = result;
}


//---------------------------------------------------------------------------

