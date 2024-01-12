// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include <moonray/rendering/displayfilter/DisplayFilter.isph>
#include <scene_rdl2/common/math/ispc/asA.isph>
#include <scene_rdl2/common/math/ispc/Math.isph>
#include <scene_rdl2/common/platform/IspcUtil.isph>

// make sure that all single ops are added below 23 (and all dual op above)
// for validation purposes
enum OpType {
    ADD =                   0,
    SUBTRACT =              1,
    MULTIPLY =              2,
    DIVIDE =                3,
    MIN =                   4,
    MAX =                   5,
    POWER =                 6,
    CROSS =                 7,
    DOT =                   8,
    MODULO =                9,
    GREATER_THAN =          10,
    GREATER_THAN_OR_EQUAL = 11,
    LESS_THAN =             12,
    LESS_THAN_OR_EQUAL =    13,
    EQUAL =                 14,
    NOT_EQUAL =             15,
    AND =                   16,
    OR =                    17,
    XOR =                   18,
    INVERT =                19,
    NORMALIZE =             20,
    ABS =                   21,
    CEIL =                  22,
    FLOOR =                 23,
    LENGTH =                24,
    SINE =                  25,
    COSINE =                26,
    ROUND =                 27,
    ACOS =                  28,
    NOT =                   29
};

enum SingleOp {
    SINGLE_OP_BEGIN = 23
};

ISPC_UTIL_EXPORT_ENUM_TO_HEADER(SingleOp);

struct OpDisplayFilter
{
    OpType mOperation;
    bool  mMask;
    bool  mInvertMask;
    float mMix;
    bool mIsSingleOp;
};

export const uniform OpDisplayFilter * uniform
OpDisplayFilter_get(const uniform DisplayFilter * uniform displayFilter)
{
    return DISPLAYFILTER_GET_ISPC_CPTR(OpDisplayFilter, displayFilter);
}

void
op(const uniform OpType operation, const varying Color& op0, const varying Color& op1, varying Color& result)
{
    switch (operation) {
    case ADD:
        result = op0 + op1;
        break;
    case SUBTRACT:
        result = op0 - op1;
        break;
    case MULTIPLY:
        result = op0 * op1;
        break;
    case DIVIDE:
        result.r = (isZero(op1.r)) ? op0.r : op0.r / op1.r;
        result.g = (isZero(op1.g)) ? op0.g : op0.g / op1.g;
        result.b = (isZero(op1.b)) ? op0.b : op0.b / op1.b;
        break;
    case MIN:
        result.r = min(op0.r, op1.r);
        result.g = min(op0.g, op1.g);
        result.b = min(op0.b, op1.b);
        break;
    case MAX:
        result.r = max(op0.r, op1.r);
        result.g = max(op0.g, op1.g);
        result.b = max(op0.b, op1.b);
        break;
    case POWER:
        result.r = pow(op0.r, op1.r);
        result.g = pow(op0.g, op1.g);
        result.b = pow(op0.b, op1.b);
        break;
    case DOT:
        result.r = dot(Vec3f_ctor(op0.r, op0.g, op0.b), Vec3f_ctor(op1.r, op1.g, op1.b));
        result.g = result.r;
        result.b = result.r;
        break;
    case CROSS:
    {
        Vec3f crossed = cross(Vec3f_ctor(op0.r, op0.g, op0.b), Vec3f_ctor(op1.r, op1.g, op1.b));
        result = Col3f_ctor(crossed.x, crossed.y, crossed.z);
    }
    break;
    case INVERT:
        result = Col3f_ctor(1.f) - op0;
        break;
    case NORMALIZE:
    {
        Vec3f normalized = normalize(Vec3f_ctor(op0.r, op0.g, op0.b));
        result = Col3f_ctor(normalized.x, normalized.y, normalized.z);
    }
    break;
    case ABS:
        result.r = abs(op0.r);
        result.g = abs(op0.g); 
        result.b = abs(op0.b);
        break;
    case CEIL:
        result.r = ceil(op0.r);
        result.g = ceil(op0.g); 
        result.b = ceil(op0.b);
        break;
    case FLOOR:
        result.r = floor(op0.r);
        result.g = floor(op0.g); 
        result.b = floor(op0.b);
        break;
    case MODULO:
        result.r = isEqual(op1.r, 0.f) ? 0.f : fmod(op0.r, op1.r);
        result.g = isEqual(op1.g, 0.f) ? 0.f : fmod(op0.g, op1.g); 
        result.b = isEqual(op1.b, 0.f) ? 0.f : fmod(op0.b, op1.b);
        break;
    case LENGTH:
    {
        float len = length(Vec3f_ctor(op0.r, op0.g, op0.b));
        result = Col3f_ctor(len);
    }
    break;
    case SINE:
        result = Col3f_ctor(sin(op0.r), sin(op0.g), sin(op0.b));
        break;
    case COSINE:
        result = Col3f_ctor(cos(op0.r), cos(op0.g), cos(op0.b));
        break;
    case ROUND:
        result.r = (float)(int)(op0.r < 0.0f ? op0.r - 0.5f : op0.r + 0.5f);
        result.g = (float)(int)(op0.g < 0.0f ? op0.g - 0.5f : op0.g + 0.5f);
        result.b = (float)(int)(op0.b < 0.0f ? op0.b - 0.5f : op0.b + 0.5f);
        break;
    case ACOS:
        result.r = dw_acos(clamp(op0.r, -1.f, 1.f));
        result.g = dw_acos(clamp(op0.g, -1.f, 1.f));
        result.b = dw_acos(clamp(op0.b, -1.f, 1.f));
        break;
    case GREATER_THAN:
        result.r = (float)(op0.r > op1.r);
        result.g = (float)(op0.g > op1.g);
        result.b = (float)(op0.b > op1.b);
        break;
    case GREATER_THAN_OR_EQUAL:
        result.r = (float)(op0.r >= op1.r);
        result.g = (float)(op0.g >= op1.g);
        result.b = (float)(op0.b >= op1.b);
        break;
    case LESS_THAN:
        result.r = (float)(op0.r < op1.r);
        result.g = (float)(op0.g < op1.g);
        result.b = (float)(op0.b < op1.b);
        break;
    case LESS_THAN_OR_EQUAL:
        result.r = (float)(op0.r <= op1.r);
        result.g = (float)(op0.g <= op1.g);
        result.b = (float)(op0.b <= op1.b);
        break;
    case EQUAL:
        result.r = (float)(op0.r == op1.r);
        result.g = (float)(op0.g == op1.g);
        result.b = (float)(op0.b == op1.b);
        break;
    case NOT_EQUAL:
        result.r = (float)(op0.r != op1.r);
        result.g = (float)(op0.g != op1.g);
        result.b = (float)(op0.b != op1.b);
        break;
    case AND:
        result.r = (op0.r != 0 && op1.r != 0) ? 1.f : 0.f;
        result.g = (op0.g != 0 && op1.g != 0) ? 1.f : 0.f;
        result.b = (op0.b != 0 && op1.b != 0) ? 1.f : 0.f; 
        break;
    case OR:
        result.r = (op0.r != 0 || op1.r != 0) ? 1.f : 0.f;
        result.g = (op0.g != 0 || op1.g != 0) ? 1.f : 0.f;
        result.b = (op0.b != 0 || op1.b != 0) ? 1.f : 0.f; 
        break;
    case NOT:
        result.r = (op0.r == 0) ? 1.f : 0.f;
        result.g = (op0.g == 0) ? 1.f : 0.f;
        result.b = (op0.b == 0) ? 1.f : 0.f;
        break;
    case XOR:
        result.r = ((op0.r != 0 || op1.r != 0) && !(op0.r != 0 && op1.r != 0)) ? 1.f : 0.f;
        result.g = ((op0.g != 0 || op1.g != 0) && !(op0.g != 0 && op1.g != 0)) ? 1.f : 0.f;
        result.b = ((op0.b != 0 || op1.b != 0) && !(op0.b != 0 && op1.b != 0)) ? 1.f : 0.f;
        break;
    default:
        // if we are given bad operation, just copy the first input
        result = op0;
        break;
    }
}

static void
filter(const uniform DisplayFilter * uniform me,
       const uniform InputBuffer * const uniform * const uniform inputBuffers,
       const varying DisplayFilterState * const uniform state,
       varying Color * uniform result)
{
    const uniform OpDisplayFilter * uniform self = OpDisplayFilter_get(me);

    // input buffers given to us are in the same order as the
    // inputs SceneObjects we returned from getInputData();
    const uniform InputBuffer const * uniform inBuffer0 = inputBuffers[0];
    const varying Color op0 = InputBuffer_getPixel(inBuffer0,
                                                   state->mOutputPixelX,
                                                   state->mOutputPixelY);
    // first check if mix is 0
    float mix = DISPLAYFILTER_mixAndMask(self->mMix,
                                         self->mMask ? inputBuffers[2] : nullptr,
                                         state->mOutputPixelX,
                                         state->mOutputPixelY,
                                         self->mInvertMask);
    if (isZero(mix)) {
        *result = op0;
        return;
    }

    if (!self->mIsSingleOp) {
        const uniform InputBuffer const * uniform inBuffer1 = inputBuffers[1];
        const varying Color op1 = InputBuffer_getPixel(inBuffer1,
                                                       state->mOutputPixelX,
                                                       state->mOutputPixelY);
        op(self->mOperation, op0, op1, *result);
    } else {
        const varying Color op1 = Color_ctor(0.f, 0.f, 0.f);
        op(self->mOperation, op0, op1, *result);
    }

    if (!isOne(mix)) {
        *result = lerp(op0, *result, mix);
    }
}

DEFINE_DISPLAY_FILTER(OpDisplayFilter, filter)

//---------------------------------------------------------------------------

