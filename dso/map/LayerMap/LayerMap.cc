// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


// This is essentially a port of Moonlight's "Layer" map shader, but with
// only two inputs:
//  A: foreground
//  B: background
//
// The compositing modes and their respective comments/descriptions are
// inspired by the moonlight shader on which this is based. According to
// the Moonlight shader's comments:
//      Most blending definitions were defined by W3C which were published and
//      available at the following url on 22 April 2004:
//          http://www.w3.org/TR/2002/WD-SVG11-20020215/masking.html#CompOp
//
//      Additional implementation help was found at:
//          http://www.vbaccelerator.com/home/VB/Code/vbMedia/Image_Processing/Compositing/article.asp
//          http://www.pegtop.net/delphi/blendmodes/

#include "attributes.cc"
#include "LayerMap_ispc_stubs.h"

#include <moonray/rendering/shading/MapApi.h>

using scene_rdl2::math::Color;

//---------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(LayerMap, scene_rdl2::rdl2::Map)

public:
    LayerMap(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name);
    ~LayerMap();
    virtual void update();

private:
    static void sample(const scene_rdl2::rdl2::Map* self, moonray::shading::TLState *tls,
                       const moonray::shading::State& state, Color* sample);

RDL2_DSO_CLASS_END(LayerMap)


//---------------------------------------------------------------------------

LayerMap::LayerMap(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mSampleFunc = LayerMap::sample;
    mSampleFuncv = (scene_rdl2::rdl2::SampleFuncv) ispc::LayerMap_getSampleFunc();
}

LayerMap::~LayerMap()
{
}

void
LayerMap::update()
{
    const int mode = get(attrMode);
    if (mode < 0 || mode > 15) {
        fatal("Unsupported composite mode");
    }
}

namespace {

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
Color over(const Color& a, const Color& b, float mask)
{
    return b * (1.0f - mask) + a;
}

// ----------------
// This operator is useful for animating a dissolve between two images.
// The alpha channel calculation has been modified from the W3C definition.
Color add(const Color& a, const Color& b)
{
    return b + a;
}

// ----------------
// The alpha channel calculation has been modified from the W3C definition.
Color subtract(const Color& a, const Color& b)
{
    return b - a;
}

// ----------------
// The resultant color is always at least as dark as either of
// the two constituent colors. Multiplying any color with
// black produces black. Multiplying any color with white leaves
// the original color unchanged.
Color multiply(const Color& a, const Color& b, float mask)
{
    return b * (1.f - mask) + a * b;
}

// ----------------
// The resultant color is always at least as light as either of
// the two constituent colors. Screening any color with white
// produces white. Screening any color with black leaves the
// original color unchanged.
Color screen(const Color& a, const Color& b)
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
Color overlay(const Color& a, const Color& b, float mask)
{
    return b * (b*mask + (2.f*a) * (scene_rdl2::math::sWhite - b)) + b * (1.f - mask);
}

// ----------------
// Like 'overlay' above, but this W3C (and probably Photoshop)
// implementation has more contrast than the GIMP version.
Color overlayContrast(const Color& a, const Color& b, float mask)
{
    Color result = b;
    for (size_t i = 0; i < 3; ++i) {
        if (b[i] < 0.5f) {
            result[i] = 2.f * a[i] * b[i] +
                        b[i] * (1.f - mask);
        } else {
            result[i] = mask * 1.f -
                        2.f * (1.f - b[i]) * (mask - a[i]) +
                        b[i] * (1.f - mask);
        }
    }
    return result;
}

// ----------------
// Selects the darker of the foreground and background colors.
Color darken(const Color& a, const Color& b, float mask)
{
    return min(a, b*mask) + b * (1.f - mask);
}

// ----------------
// Selects the lighter of the foreground and background colors.
Color lighten(const Color& a, const Color& b, float mask)
{
    return max(a, b*mask) + b * (1.f - mask);
}

// ----------------
// Brightens the background color to reflect the foreground color.
Color colorDodge(const Color& a, const Color& b, float mask)
{
    Color result = b;
    for (size_t i = 0; i < 3; ++i) {
        if (a[i] + b[i] * mask >= mask) {
            result[i] = mask + b[i] * (1.f - mask);
        } else {
            float tmp = 1.f - a[i]/mask; // assumes mask is != 0
            if (!scene_rdl2::math::isZero(tmp)) {
                tmp = b[i] * mask / tmp;
            }
            result[i] = tmp + b[i] * (1.f - mask);
        }
    }
    return result;
}

// ----------------
// Darkens the background color to reflect the foreground color.
Color colorBurn(const Color& a, const Color& b, float mask)
{
    Color result = b;
    for (size_t i = 0; i < 3; ++i) {
        if (a[i] + b[i] * mask <= mask) {
            result[i] = b[i] * (1.f - mask);
        } else {
            float tmp;
            if (!scene_rdl2::math::isZero(a[i])) {
                tmp = mask * (a[i] + b[i]*mask - mask) / a[i];
            } else {
                tmp = 0.f;
            }

            result[i] = tmp + b[i] * (1.f - mask);
        }
    }
    return result;
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
Color hardLight(const Color& a, const Color& b, float mask)
{
    Color result = b;
    for (size_t i = 0; i < 3; ++i) {
        if (2.f * a[i] < mask) {
            result[i] = 2.f * a[i] * b[i] +
                        b[i] * (1.f - mask);
        } else {
            result[i] = mask -
                        2.0 * (1.f - b[i]) *
                        (mask - a[i]) +
                        b[i] * (1.f - mask);
        }
    }
    return result;
}

// ----------------
// Using method from http://www.pegtop.net/delphi/blendmodes/ which is
// supposedly what Photoshop uses.  Other implementations looked
// exactly like Overlay to me.
Color softLight(const Color& a, const Color& b, float mask)
{
    Color result = b;
    for (size_t i = 0; i < 3; ++i) {
        if (2.f * a[i] < mask) {
            result[i] = 2.f * b[i] * a[i] +
                        b[i] * b[i] * (mask - 2.f * a[i]) +
                        b[i] * (1.f - mask);
        } else {
            float tmp = b[i] * mask;
            if (tmp < 0.f) tmp = 0.f;

            result[i] = sqrt(tmp) *
                        (2.f * a[i] - mask) + (2.f * tmp) *
                        (mask - a[i]) +
                        b[i] * (1.f - mask);
        }
    }
    return result;
}

// ----------------
// Subtracts the darker of the two constituent colors from the lighter.
Color difference(const Color& a, const Color&b, float mask)
{
    return a + b - 2.f * min(a, b*mask);
}

// ----------------
// Produces an effect similar to that of 'difference', but appears as
// lower contrast.
Color exclusion(const Color& a, const Color&b, float mask)
{
    return (a + b * mask - 2.f * a * b) + b * (1.f - mask);
}

// ----------------
Color composite(const Color& a, const Color& b, float mask, int mode)
{
    Color result = scene_rdl2::math::sBlack;

    switch(mode) {
    case ispc::CompositeMode::COMPOSITE_OVER :
        result = over(a, b, mask);
        break;
    case ispc::CompositeMode::COMPOSITE_ADD :
        result = add(a, b);
        break;
    case ispc::CompositeMode::COMPOSITE_SUBTRACT :
        result = subtract(a, b);
        break;
    case ispc::CompositeMode::COMPOSITE_MULTIPLY :
        result = multiply(a, b, mask);
        break;
    case ispc::CompositeMode::COMPOSITE_SCREEN :
        result = screen(a, b);
        break;
    case ispc::CompositeMode::COMPOSITE_OVERLAY :
        result = overlay(a, b, mask);
        break;
    case ispc::CompositeMode::COMPOSITE_OVERLAY_CONTRAST :
        result = overlayContrast(a, b, mask);
        break;
    case ispc::CompositeMode::COMPOSITE_DARKEN :
        result = darken(a, b, mask);
        break;
    case ispc::CompositeMode::COMPOSITE_LIGHTEN :
        result = lighten(a, b, mask);
        break;
    case ispc::CompositeMode::COMPOSITE_COLOR_DODGE :
        result = colorDodge(a, b, mask);
        break;
    case ispc::CompositeMode::COMPOSITE_COLOR_BURN :
        result = colorBurn(a, b, mask);
        break;
    case ispc::CompositeMode::COMPOSITE_HARD_LIGHT :
        result = hardLight(a, b, mask);
        break;
    case ispc::CompositeMode::COMPOSITE_SOFT_LIGHT :
        result = softLight(a, b, mask);
        break;
    case ispc::CompositeMode::COMPOSITE_DIFFERENCE :
        result = difference(a, b, mask);
        break;
    case ispc::CompositeMode::COMPOSITE_EXCLUSION :
        result = exclusion(a, b, mask);
        break;
    };

    return result;
}

} // namespace

void
LayerMap::sample(const scene_rdl2::rdl2::Map* self, moonray::shading::TLState *tls,
                 const moonray::shading::State& state, Color* sample)
{
    const LayerMap* me = static_cast<const LayerMap*>(self);
    const float mask = scene_rdl2::math::saturate(evalFloat(me, attrMask, tls, state));

    const int mode = me->get(attrMode);
    if (mode == 0 || scene_rdl2::math::isZero(mask)) {
        // no compositing required, return background
        *sample = evalColor(me, attrB, tls, state);
        return;
    }
    if (mode == 1 && scene_rdl2::math::isOne(mask)) {
        // 'over' mode, no compositing required, return foreground
        *sample = evalColor(me, attrA, tls, state);
        return;
    }

    // perform compositing operation
    const Color a = evalColor(me, attrA, tls, state) * mask;
    const Color b = evalColor(me, attrB, tls, state);
    *sample = composite(a, b, mask, mode);
}


//---------------------------------------------------------------------------

