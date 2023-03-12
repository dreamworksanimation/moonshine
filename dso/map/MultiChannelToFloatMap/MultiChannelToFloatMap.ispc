// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

/// @file MultiChannelToFloatMap.ispc

/// Converts Color input to a float using the following modes:
/// R: the R component
/// G: the G component
/// B: the B component
/// C: the Cyan basis vector    (0, 1, 1)
/// M: the Magenta basis vector (1, 0, 1)
/// Y: the Yellow basis vector  (1, 1, 0)
/// W: the White basis vector   (1, 1, 1)
/// The output is a Color that its RGB value is filled by the converted float

#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>

enum MultiChannelToFloatMode {
    R,
    G,
    B,
    C,
    M,
    Y,
    W,
    K
};

ISPC_UTIL_EXPORT_ENUM_TO_HEADER(MultiChannelToFloatMode);

struct MultiChannelToFloatMap
{
    uniform int mMode;
};

ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(MultiChannelToFloatMap);

static varying float
dotColor(const varying Color& a,
         const uniform Color& b)
{
    return a.r * b.r + a.g * b.g + a.b * b.b;
}

static varying float
dotColor(const varying Color& a,
         const varying Color& b)
{
    return a.r * b.r + a.g * b.g + a.b * b.b;
}

static varying float
lengthSqColor(const varying Color& input)
{
    return dotColor(input, input);
}

static varying MultiChannelToFloatMode
categorizeColor(const varying Color& input, varying Color& result)
{
    result = sBlack;

    if (isEqual(input, sBlack)) {
        return K;
    }

    static const uniform Color red =       {1.f, 0.f, 0.f};
    static const uniform Color green =     {0.f, 1.f, 0.f};
    static const uniform Color blue =      {0.f, 0.f, 1.f};
    static const uniform Color cyan =      {0.f, 1.f, 1.f};
    static const uniform Color magenta =   {1.f, 0.f, 1.f};
    static const uniform Color yellow =    {1.f, 1.f, 0.f};

    // Calculate the length of the vector rejection from the input color to each
    // of the 7 color axes.
    const varying Color projOnRed     = dotColor(input, red) * red;
    const varying Color projOnGreen   = dotColor(input, green) * green;
    const varying Color projOnBlue    = dotColor(input, blue) * blue;
    const varying Color projOnCyan    = (dotColor(input, cyan) / 2.f) * cyan;
    const varying Color projOnMagenta = (dotColor(input, magenta) / 2.f) * magenta;
    const varying Color projOnYellow  = (dotColor(input, yellow) / 2.f) * yellow;
    const varying Color projOnWhite   = (dotColor(input, sWhite) / 3.f) * sWhite;
    const varying float distSqFromRed     = lengthSqColor(input - projOnRed);
    const varying float distSqFromGreen   = lengthSqColor(input - projOnGreen);
    const varying float distSqFromBlue    = lengthSqColor(input - projOnBlue);
    const varying float distSqFromCyan    = lengthSqColor(input - projOnCyan);
    const varying float distSqFromMagenta = lengthSqColor(input - projOnMagenta);
    const varying float distSqFromYellow  = lengthSqColor(input - projOnYellow);
    const varying float distSqFromWhite   = lengthSqColor(input - projOnWhite);

    // Generate weight based on linear combination of distance to the two closest
    // color basis vectors.
    const varying float distances[7] = {distSqFromRed, distSqFromGreen, distSqFromBlue,
            distSqFromCyan, distSqFromMagenta, distSqFromYellow, distSqFromWhite};

    varying float min = 1.f;
    varying float secondMin = 1.f;
    for (uniform unsigned int i = 0; i < 7; ++i) {
        if (distances[i] <= min) {
            secondMin = min;
            min = distances[i];
        } else if (distances[i] < secondMin) {
            secondMin = distances[i];
        }
    }

    // Use inverse weight. The closer to the primary color axis (min) and the further
    // from the secondary color axis (secondMin) the greater the weight.
    //     If min == 0, weight == 1
    //     Lowest possible weight should be 0.5 when min == secondMin
    if (isZero(min + secondMin)) {
        // zero vector
        return K;
    }
    // Technically should take square root since these values are lengths squared
    // but in practice there is almost no difference
    const varying float weight = secondMin / (min + secondMin);

    // Return the color axis that is closest to the input color
    // Set result to the one of the non-zero components of the weighted projection
    if (isEqual(min, distSqFromRed)) {
        const varying Color weightedProjection = weight * projOnRed;
        result = Color_ctor(weightedProjection.r);
        return R;
    } else if (isEqual(min, distSqFromGreen)) {
        const varying Color weightedProjection = weight * projOnGreen;
        result = Color_ctor(weightedProjection.g);
        return G;
    } else if (isEqual(min, distSqFromBlue)) {
        const varying Color weightedProjection = weight * projOnBlue;
        result = Color_ctor(weightedProjection.b);
        return B;
    } else if (isEqual(min, distSqFromCyan)) {
        const varying Color weightedProjection = weight * projOnCyan;
        result = Color_ctor(weightedProjection.g);
        return C;
    } else if (isEqual(min, distSqFromMagenta)) {
        const varying Color weightedProjection = weight * projOnMagenta;
        result = Color_ctor(weightedProjection.r);
        return M;
    } else if (isEqual(min, distSqFromYellow)) {
        const varying Color weightedProjection = weight * projOnYellow;
        result = Color_ctor(weightedProjection.r);
        return Y;
    } else if (isEqual(min, distSqFromWhite)) {
        const varying Color weightedProjection = weight * projOnWhite;
        result = Color_ctor(weightedProjection.r);
        return W;
    } else {
        return K;
    }
}

static Color
sample(const uniform Map*            uniform  map,
             uniform ShadingTLState* uniform  tls,
       const varying State&                   state)
{
    const uniform MultiChannelToFloatMap * uniform me =
        MAP_GET_ISPC_CPTR(MultiChannelToFloatMap, map);
    const varying Color input = clamp(evalAttrInput(map, tls, state), 0.f, 1.f);
    const varying float tolerance = clamp(evalAttrTolerance(map, tls, state), 0.f, 1.f);

    varying Color result;
    const varying MultiChannelToFloatMode colorCategory = categorizeColor(input, result);

    Color sample = sBlack;
    if ((1.f - result.r) > tolerance) {
        return sample;
    }

    switch (me->mMode) {
    case R:
    {
        if (colorCategory == R) {
            sample = result;
        }
        break;
    }
    case G:
    {
        if (colorCategory == G) {
            sample = result;
        }
        break;
    }
    case B:
    {
        if (colorCategory == B) {
            sample = result;
        }
        break;
    }
    case C:
    {
        if (colorCategory == C) {
            sample = result;
        }
        break;
    }
    case M:
    {
        if (colorCategory == M) {
            sample = result;
        }
        break;
    }
    case Y:
    {
        if (colorCategory == Y) {
            sample = result;
        }
        break;
    }
    case W:
    {
        if (colorCategory == W) {
            sample = result;
        }
        break;
    }
    default:
        break;
    }
    return sample;
}

DEFINE_MAP_SHADER(MultiChannelToFloatMap, sample)

