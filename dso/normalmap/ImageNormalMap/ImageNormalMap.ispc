// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.isph"

#include <moonray/rendering/shading/ispc/MapApi.isph>
#include <moonray/rendering/shading/ispc/BasicTexture.isph>
#include <moonray/rendering/shading/ispc/UdimTexture.isph>

enum InputTextureType {
    ST = 0,
    INPUT_TEXTURE_COORDINATES = 1,
};
ISPC_UTIL_EXPORT_ENUM_TO_HEADER(InputTextureType);

struct ImageNormalMap
{
    uniform Color mFatalColor;
    const uniform BASIC_TEXTURE_Data * uniform mTexture;
    const uniform UDIM_TEXTURE_Data * uniform mUdimTexture;
    uniform int mReversedNormalsIndx;
};

ISPC_UTIL_EXPORT_UNIFORM_STRUCT_TO_HEADER(ImageNormalMap);

bool
isOutside(const varying Vec2f& uv) {
    return (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f);
}

static void
rotateTexCoords(uniform float theta, uniform const Vec2f& rotationCenter,
        varying Vec2f& st, varying float& dsdx, varying float& dsdy,
        varying float& dtdx, varying float& dtdy)
{
    const uniform Mat3f R = Mat3f_ctor( cos(theta), -sin(theta),    0,
                                        sin(theta), cos(theta),     0,
                                        0,          0,              1);
    Vec3f st3 = Vec3f_ctor(st.x, st.y, 0.f);
    const uniform Vec3f rotationCenter3 = Vec3f_ctor(rotationCenter.x, rotationCenter.y, 0.f);
    // Translate rotation center to origin.
    st3 = st3 - rotationCenter3;
    // Rotate.
    st3 = st3 * R;
    // Translate rotation center back.
    st3 = st3 + rotationCenter3;
    st.x = st3.x;
    st.y = st3.y;

    // Rotate derivatives.
    Vec3f dsdxy3 = Vec3f_ctor(dsdx, dsdy, 0.f);
    Vec3f dtdxy3 = Vec3f_ctor(dtdx, dtdy, 0.f);
    dsdxy3 = dsdxy3 * transpose(R);
    dtdxy3 = dtdxy3 * transpose(R);
    dsdx = dsdxy3.x;
    dsdy = dsdxy3.y;
    dtdx = dtdxy3.x;
    dtdy = dtdxy3.y;
}

static Vec3f
sampleNormal(const uniform NormalMap* uniform map,
             uniform ShadingTLState* uniform tls,
             const varying State& state)
{
    const uniform ImageNormalMap* uniform me = MAP_GET_ISPC_PTR(ImageNormalMap, map);

    Vec2f st;
    float dsdx, dtdx, dsdy, dtdy;

    switch (getAttrTextureEnum(map)) {
    case ST:
        {
            st = state.mSt;
            dsdx = state.mdSdx;
            dtdx = state.mdTdx;
            dsdy = state.mdSdy;
            dtdy = state.mdTdy;
        }
        break;
    case INPUT_TEXTURE_COORDINATES:
        {
            const Vec3f uvw = evalAttrInputTextureCoordinate(map, tls, state);
            st = asAVec2f(uvw);
            dsdx = dsdy = dtdx = dtdy = 0.0;

            // A negative w value signals an "invalid" texture coordinate. This is
            // purely a convention.  An example usage would be where a tex coordinate
            // generator map (eg. camera projection) wants to signal to Image map that
            // the coordinate should be considered out of range, and thus we should
            // simply return black.
            if (uvw.z < 0.f) {
                return Vec3f_ctor(0.0f);
            }
        }
        break;
    default:
        st = state.mSt;
        break;
    }

    // If the "wrap around" parameter is false and we are outside the
    // 0 to 1 range, then just return the shading normal.
    if (!getAttrWrapAround(map) && isOutside(st)) {
        const Vec3f& N = getN(state);
        return Vec3f_ctor(N.x, N.y, N.z);
    }

    varying Col4f tx;
    if (me->mTexture) {
        // rotation and scaling only for non-udim case
        const Vec2f offset = getAttrOffset(map);
        const Vec2f scale  = getAttrScale(map);
        const uniform Vec2f rotationCenter = getAttrRotationCenter(map);
        // Rotate coords and derivatives.
        const uniform float theta = deg2rad(getAttrRotationAngle(map));
        if (!isZero(theta)) {
            rotateTexCoords(theta, rotationCenter, st, dsdx, dsdy, dtdx, dtdy);
        }
        // Scale and translate coords.
        st.x = scale.x * st.x + offset.x;
        st.y = scale.y * st.y + offset.y;

        // Invert t coord.
        st.y = 1.0 - st.y;
        // Set and scale derivatives.
        float derivatives[4] = { dsdx * scale.x,
                                -dtdx * scale.x,
                                 dsdy * scale.y,
                                -dtdy * scale.y };

        tx = BASIC_TEXTURE_sample(me->mTexture, tls, state, st, derivatives);
    } else if (me->mUdimTexture) {
        // compute udim index
        const varying int udim = UDIM_TEXTURE_compute_udim(me->mUdimTexture, tls, st);
        if (udim == -1) {
            // UDIM_TEXTURE_compute_udim logs the invalid udim error
            return Vec3f_ctor(me->mUdimTexture->mFatalColor.r,
                              me->mUdimTexture->mFatalColor.g,
                              me->mUdimTexture->mFatalColor.b);
        }

        // take fractional parts of st
        st.x = st.x - (int)st.x;
        st.y = st.y - (int)st.y;

        // Invert t coord.
        st.y = 1.0 - st.y;
        // Set and scale derivatives.
        float derivatives[4] = { dsdx,
                                -dtdx,
                                 dsdy,
                                -dtdy };

        tx = UDIM_TEXTURE_sample(me->mUdimTexture, tls, state, udim, st, derivatives);
    }

    Vec3f normal = Vec3f_ctor(tx.r, tx.g, tx.b);

    // Validate normal
    if (isnan(normal.x) ||
        isnan(normal.y) ||
        isnan(normal.z) ||
        !isfinite(normal.x) ||
        !isfinite(normal.y) ||
        !isfinite(normal.z) ||
        isZero(normal)) {

        return state.mN;
    }

    // Re-center sampled normal from [0, 1] --> [-1, 1]
    // z is usually encoded from [0.5, 1.0] so that this re-centering
    // keeps it positive (in [0, 1])
    const int normalMapEncoding = getAttrNormalEncoding(map);
    if (normalMapEncoding == 0) { // [0,1]
        normal = 2.0f * normal - Vec3f_ctor(1.0f, 1.0f, 1.0f);
    }

    // Transform from tangent space to shade space
    varying bool reversedNormals = false;
    if (isProvided(state, me->mReversedNormalsIndx)) {
        reversedNormals = getBoolAttribute(tls, state, me->mReversedNormalsIndx);
    }
    const Vec3f statedPds = reversedNormals ? state.mdPds * -1.0f : state.mdPds * 1.0f;
    const ReferenceFrame frame = ReferenceFrame_ctor(getN(state), normalize(statedPds));
    const Vec3f renderN = localToGlobal(frame, normal);

    // Don't normalize vector, need unnormalized length for
    // Toksvig normal anti-aliasing technique in Dwa materials.
    return renderN;
}

DEFINE_NORMALMAP_SHADER(ImageNormalMap, sampleNormal)

