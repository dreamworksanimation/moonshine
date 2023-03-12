// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file LayerMap.ispc

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

enum CompositeMode {
    COMPOSITE_OFF               = 0,
    COMPOSITE_OVER              = 1,
    COMPOSITE_ADD               = 2,
    COMPOSITE_SUBTRACT          = 3,
    COMPOSITE_MULTIPLY          = 4,
    COMPOSITE_SCREEN            = 5,
    COMPOSITE_OVERLAY           = 6,
    COMPOSITE_OVERLAY_CONTRAST  = 7,
    COMPOSITE_DARKEN            = 8,
    COMPOSITE_LIGHTEN           = 9,
    COMPOSITE_COLOR_DODGE       = 10,
    COMPOSITE_COLOR_BURN        = 11,
    COMPOSITE_HARD_LIGHT        = 12,
    COMPOSITE_SOFT_LIGHT        = 13,
    COMPOSITE_DIFFERENCE        = 14,
    COMPOSITE_EXCLUSION         = 15
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(CompositeMode);

/* struct LayerMap */
/* { */
/* }; */




// -----------------------------------------------------
// TODO: Consider moving these to a library so they can
// be used in other shaders.
//
// NOTE: Each of these compositing functions make two assumptions:
// 1) the 'a' (foreground) color has already been premultiplied with 'mask'
// 2) mask != 0  ( no check is performed inside these functions )
// -----------------------------------------------------

// ----------------
// Typical "over" (associative) operation proposed by Bruce Wallace &
// Marc Levoy and Tom Porter & Tom Duff (SIGGRAPH '84)
inline Color
over(const Color& a, const Color& b, float mask)
{
    return b*(1.0f - mask) + a;
}

// ----------------
// This operator is useful for animating a dissolve between two images.
// The alpha channel calculation has been modified from the W3C definition.
inline Color
add(const Color& a, const Color& b, float mask)
{
    return b + a;
}

// ----------------
// The alpha channel calculation has been modified from the W3C definition.
inline Color
subtract(const Color& a, const Color& b, float /*mask*/)
{
    return b - a;
}

// ----------------
// The resultant color is always at least as dark as either of
// the two constituent colors. Multiplying any color with
// black produces black. Multiplying any color with white leaves
// the original color unchanged.
inline Color
multiply(const Color& a, const Color& b, float mask)
{
    return b * (1.f - mask) + a * b;
}

// ----------------
// The resultant color is always at least as light as either of
// the two constituent colors. Screening any color with white
// produces white. Screening any color with black leaves the
// original color unchanged.
inline Color
screen(const Color& a, const Color& b, float /*mask*/)
{
    return a + b - a * b;
}

// ----------------
// Multiplies or screens the colors, dependent on the background
// color. foreground colors overlay the background whilst preserving
// its highlights and shadows. The background color is not replaced,
// but is mixed with the foreground color to reflect the lightness or
// darkness of the background.
//
// This is actually GIMP's implementation, not the one from W3C:
inline Color
overlay(const Color& a, const Color& b, float mask)
{
    return b * (b*mask + (2.f*a) * (sWhite - b)) + b * (1.f - mask);
}

// ----------------
// Like 'overlay' above, but this W3C (and probably Photoshop)
// implementation has more contrast than the GIMP version.
inline Color
overlayContrast(const Color& a, const Color& b, float mask)
{
    // Color type has no index operator[], so we'll pointers
    const varying float * uniform A = asArray(a);
    const varying float * uniform B = asArray(b);
    float res[3];

    for (size_t i = 0; i < 3; ++i) {
        if (B[i] < 0.5f) {
            res[i] = 2.f * A[i] * B[i] +
                     B[i] * (1.f - mask);
        } else {
            res[i] = mask * 1.f -
                     2.f * (1.f - B[i]) * (mask - A[i]) +
                     B[i] * (1.f - mask);
        }
    }
    return Color_ctor(res[0], res[1], res[2]);;
}

// ----------------
// Selects the darker of the foreground and background colors.
inline Color
darken(const Color& a, const Color& b, float mask)
{
    return min(a, b*mask) + b * (1.f - mask);
}

// ----------------
// Selects the lighter of the foreground and background colors.
inline Color
lighten(const Color& a, const Color& b, float mask)
{
    return max(a, b*mask) + b * (1.f - mask);
}

// ----------------
// Brightens the background color to reflect the foreground color.
inline Color
colorDodge(const Color& a, const Color& b, float mask)
{
    // Color type has no index operator[], so we'll pointers
    const varying float * uniform A = asArray(a);
    const varying float * uniform B = asArray(b);
    float res[3];

    for (size_t i = 0; i < 3; ++i) {
        if (A[i] + B[i] * mask >= mask) {
            res[i] = mask + B[i] * (1.f - mask);
        } else {
            float tmp = 1.f - A[i]/mask; // assumes mask is != 0
            if (!isZero(tmp)) {
                tmp = B[i] * mask / tmp;
            }
            res[i] = tmp + B[i] * (1.f - mask);
        }
    }
    return Color_ctor(res[0], res[1], res[2]);;
}

// ----------------
// Darkens the background color to reflect the foreground color.
inline Color
colorBurn(const Color& a, const Color& b, float mask)
{
    // Color type has no index operator[], so we'll pointers
    const varying float * uniform A = asArray(a);
    const varying float * uniform B = asArray(b);
    float res[3];

    for (size_t i = 0; i < 3; ++i) {
        if (A[i] + B[i] * mask <= mask) {
            res[i] = B[i] * (1.f - mask);
        } else {
            float tmp;
            if (!isZero(A[i])) {
                tmp = mask * (A[i] + B[i]*mask - mask) / A[i];
            } else {
                tmp = 0.f;
            }

            res[i] = tmp + B[i] * (1.f - mask);
        }
    }
    return Color_ctor(res[0], res[1], res[2]);;
}

// ----------------
// Multiplies or screens the colors, dependent on the foreground color
// value. If the foreground color is lighter than 0.5, the background
// is lightened as if it were screened. If the foreground color is darker
// than 0.5, the background is darkened, as if it were multiplied.
// The degree of lightening or darkening is proportional to the
// difference between the foreground colour and 0.5. If it is equal to
// 0.5 the background is unchanged. Painting with pure black or
// white produces black or white.
inline Color
hardLight(const Color& a, const Color& b, float mask)
{
    // Color type has no index operator[], so we'll pointers
    const varying float * uniform A = asArray(a);
    const varying float * uniform B = asArray(b);
    float res[3];

    for (size_t i = 0; i < 3; ++i) {
        if (2.f * A[i] < mask) {
            res[i] = 2.f * A[i] * B[i] +
                     B[i] * (1.f - mask);
        } else {
            res[i] = mask -
                     2.0 * (1.f - B[i]) *
                     (mask - A[i]) +
                     B[i] * (1.f - mask);
        }
    }
    return Color_ctor(res[0], res[1], res[2]);;
}

// ----------------
// Using method from http://www.pegtop.net/delphi/blendmodes/ which is
// supposedly what Photoshop uses.  Other implementations looked
// exactly like Overlay to me.
inline Color
softLight(const Color& a, const Color& b, float mask)
{
    // Color type has no index operator[], so we'll pointers
    const varying float * uniform A = asArray(a);
    const varying float * uniform B = asArray(b);
    float res[3];

    for (size_t i = 0; i < 3; ++i) {
        if (2.f * A[i] < mask) {
            res[i] = 2.f * B[i] * A[i] +
                     B[i] * B[i] * (mask - 2.f * A[i]) +
                     B[i] * (1.f - mask);
        } else {
            float tmp = B[i] * mask;
            if (tmp < 0.f) tmp = 0.f;

            res[i] = sqrt(tmp) *
                     (2.f * A[i] - mask) + (2.f * tmp) *
                     (mask - A[i]) +
                     B[i] * (1.f - mask);
        }
    }
    return Color_ctor(res[0], res[1], res[2]);;
}

// ----------------
// Subtracts the darker of the two constituent colors from the lighter.
inline Color
difference(const Color& a, const Color&b, float mask)
{
    return a + b - 2.f * min(a, b*mask);
}

// ----------------
// Produces an effect similar to that of 'difference', but appears as
// lower contrast.
inline Color
exclusion(const Color& a, const Color&b, float mask)
{
    return (a + b*mask - 2.f * a * b) + b * (1.f - mask);
}

// ----------------
inline Color
composite(const varying Color& a, const varying Color& b,
          varying float mask, uniform int mode)
{
    Color result = sBlack;

    switch(mode) {
    case COMPOSITE_OVER :
        result = over(a, b, mask);
        break;
    case COMPOSITE_ADD :
        result = add(a, b, mask);
        break;
    case COMPOSITE_SUBTRACT :
        result = subtract(a, b, mask);
        break;
    case COMPOSITE_MULTIPLY :
        result = multiply(a, b, mask);
        break;
    case COMPOSITE_SCREEN :
        result = screen(a, b, mask);
        break;
    case COMPOSITE_OVERLAY :
        result = overlay(a, b, mask);
        break;
    case COMPOSITE_OVERLAY_CONTRAST :
        result = overlayContrast(a, b, mask);
        break;
    case COMPOSITE_DARKEN :
        result = darken(a, b, mask);
        break;
    case COMPOSITE_LIGHTEN :
        result = lighten(a, b, mask);
        break;
    case COMPOSITE_COLOR_DODGE :
        result = colorDodge(a, b, mask);
        break;
    case COMPOSITE_COLOR_BURN :
        result = colorBurn(a, b, mask);
        break;
    case COMPOSITE_HARD_LIGHT :
        result = hardLight(a, b, mask);
        break;
    case COMPOSITE_SOFT_LIGHT :
        result = softLight(a, b, mask);
        break;
    case COMPOSITE_DIFFERENCE :
        result = difference(a, b, mask);
        break;
    case COMPOSITE_EXCLUSION :
        result = exclusion(a, b, mask);
        break;
    };

    return result;
}

static Color
sample(const uniform Map *uniform map,
       uniform ShadingTLState * uniform tls,
       const varying State &state)
{
    const varying float mask = saturate(evalAttrMask(map, tls, state));

    const uniform int mode = getAttrMode(map);
    if (mode == COMPOSITE_OFF || isZero(mask)) {
        // no compositing required, return background
        return evalAttrB(map, tls, state);
    }

    if (mode == COMPOSITE_OVER && isOne(mask)) {
        // no compositing required, return foreground
        return evalAttrA(map, tls, state);
    }

    // perform compositing operation
    const varying Color a = evalAttrA(map, tls, state) * mask;
    const varying Color b = evalAttrB(map, tls, state);
    return composite(a, b, mask, mode);
}

DEFINE_MAP_SHADER(LayerMap, sample)

