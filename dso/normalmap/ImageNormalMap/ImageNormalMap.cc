// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.cc"
#include "ImageNormalMap_ispc_stubs.h"

#include <moonray/common/mcrt_macros/moonray_static_check.h>
#include <moonray/rendering/shading/BasicTexture.h>
#include <moonray/rendering/shading/UdimTexture.h>
#include <moonray/rendering/shading/MapApi.h>

#include <memory>

using namespace scene_rdl2::math;
using namespace scene_rdl2;

namespace {
bool
isOutside(const Vec2f& uv) {
    return (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f);
}

void rotateTexCoords(float theta, const Vec2f& rotationCenter,
                     Vec2f& st, float& dsdx, float& dsdy, float& dtdx, float& dtdy)
{
    const Mat3f R(cos(theta), -sin(theta),     0,
                  sin(theta),  cos(theta),     0,
                  0,           0,     1);
    Vec3f st3(st.x, st.y, 0.f);
    const Vec3f rotationCenter3(rotationCenter.x, rotationCenter.y, 0.f);
    // Translate rotation center to origin.
    st3 -= rotationCenter3;
    // Rotate.
    st3 = st3 * R;
    // Translate rotation center back.
    st3 += rotationCenter3;
    st.x = st3.x;
    st.y = st3.y;

    // Rotate derivatives.
    Vec3f dsdxy3(dsdx, dsdy, 0.f);
    Vec3f dtdxy3(dtdx, dtdy, 0.f);
    dsdxy3 = dsdxy3 * R.transposed();
    dtdxy3 = dtdxy3 * R.transposed();
    dsdx = dsdxy3.x;
    dsdy = dsdxy3.y;
    dtdx = dtdxy3.x;
    dtdy = dtdxy3.y;
}
}

//----------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(ImageNormalMap, scene_rdl2::rdl2::NormalMap)

public:
    ImageNormalMap(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name);
    ~ImageNormalMap() override;
    void update() override;

private:
    static void sampleNormal(const scene_rdl2::rdl2::NormalMap* self, moonray::shading::TLState* tls, const moonray::shading::State& state, Vec3f* sample);
    void updateUdimTexture();
    void updateBasicTexture();

    ispc::ImageNormalMap mIspc; // must be first member
    std::unique_ptr<moonray::shading::BasicTexture> mTexture;
    std::unique_ptr<moonray::shading::UdimTexture> mUdimTexture;

RDL2_DSO_CLASS_END(ImageNormalMap)

//----------------------------------------------------------------------------

ImageNormalMap::ImageNormalMap(const scene_rdl2::rdl2::SceneClass& sceneClass, const std::string& name)
    : Parent(sceneClass, name)
{
    mSampleNormalFunc = ImageNormalMap::sampleNormal;
    mSampleNormalFuncv = (scene_rdl2::rdl2::SampleNormalFuncv) ispc::ImageNormalMap_getSampleFunc();

    const scene_rdl2::rdl2::SceneVariables &sv = getSceneClass().getSceneContext()->getSceneVariables();
    asCpp(mIspc.mFatalColor) = sv.get(scene_rdl2::rdl2::SceneVariables::sFatalColor);

    mTexture = std::make_unique<moonray::shading::BasicTexture>(this, sLogEventRegistry);
    mIspc.mTexture = &mTexture->getBasicTextureData();
}

ImageNormalMap::~ImageNormalMap()
{
}

void
ImageNormalMap::updateUdimTexture()
{
    mTexture = nullptr;
    mIspc.mTexture = nullptr;
    bool needsUpdate = false;
    if (!mUdimTexture) {
        needsUpdate = true;
        mUdimTexture = std::make_unique<moonray::shading::UdimTexture>(this);
        mIspc.mUdimTexture = &mUdimTexture->getUdimTextureData();
    }
    if (needsUpdate ||
        hasChanged(attrTexture) ||
        hasChanged(attrWrapAround)) {

        std::string errorStr;

        moonray::shading::WrapType wrapS, wrapT;
        if (get(attrWrapAround)) {
            wrapS = moonray::shading::WrapType::Periodic;
            wrapT = moonray::shading::WrapType::Periodic;
        } else {
            wrapS = moonray::shading::WrapType::Clamp;
            wrapT = moonray::shading::WrapType::Clamp;
        }

        if (!mUdimTexture->update(this,
                                  sLogEventRegistry,
                                  get(attrTexture),
                                  ispc::TEXTURE_GAMMA_OFF,
                                  wrapS,
                                  wrapT,
                                  false,                        // use default color
                                  sBlack,                       // default color
                                  asCpp(mIspc.mFatalColor),
                                  errorStr)) {
            fatal(errorStr);
            return;
        }
    }
}

void
ImageNormalMap::updateBasicTexture()
{
    mUdimTexture = nullptr;
    mIspc.mUdimTexture = nullptr;
    bool needsUpdate = false;
    if (!mTexture) {
        needsUpdate = true;
        mTexture = std::make_unique<moonray::shading::BasicTexture>(this, sLogEventRegistry);
        mIspc.mTexture = &mTexture->getBasicTextureData();
    }
    if (needsUpdate ||
        hasChanged(attrTexture) ||
        hasChanged(attrWrapAround)) {
        std::string errorStr;
        Vec3f defaultValue = get(attrDefaultValue);

        moonray::shading::WrapType wrapS, wrapT;
        if (get(attrWrapAround)) {
            wrapS = moonray::shading::WrapType::Periodic;
            wrapT = moonray::shading::WrapType::Periodic;
        } else {
            wrapS = moonray::shading::WrapType::Clamp;
            wrapT = moonray::shading::WrapType::Clamp;
        }

        if (!mTexture->update(get(attrTexture),
                              ispc::TEXTURE_GAMMA_OFF,
                              wrapS,
                              wrapT,
                              get(attrUseDefaultValue),
                              Color(defaultValue.x, defaultValue.y, defaultValue.z),
                              asCpp(mIspc.mFatalColor),
                              errorStr)) {
            fatal(errorStr);
            return;
        }
    }
}

void
ImageNormalMap::update()
{
    const std::string filename = get(attrTexture);
    const std::size_t udimPos = filename.find("<UDIM>");
    const bool isUdim = udimPos != std::string::npos;

    if (isUdim) {
        // Udim update, if needed
        updateUdimTexture();
    } else {
        // Non-udim update, if needed
        updateBasicTexture();
    }

    if ((mTexture && !mTexture->isValid()) ||
        (mUdimTexture && !mUdimTexture->isValid())) {
        fatal("texture: ", filename, " is not valid");
    }

    // Get whether or not the normals have been reversed
    mOptionalAttributes.push_back(moonray::shading::StandardAttributes::sReversedNormals);
    mIspc.mReversedNormalsIndx = moonray::shading::StandardAttributes::sReversedNormals.getIndex();
}

void
ImageNormalMap::sampleNormal(const scene_rdl2::rdl2::NormalMap* self,
                             moonray::shading::TLState* tls,
                             const moonray::shading::State& state,
                             Vec3f* sample)
{
    const ImageNormalMap* me = static_cast<const ImageNormalMap*>(self);

    Vec2f st;
    float dsdx = state.getdSdx();
    float dsdy = state.getdSdy();
    float dtdx = state.getdTdx();
    float dtdy = state.getdTdy();

    switch ( me->get( attrTextureEnum ) ) {
    case ispc::ST:
        st = state.getSt();
        break;
    case ispc::INPUT_TEXTURE_COORDINATES:
        {
            Vec3f uvw = evalVec3f(me, attrInputTextureCoordinate, tls, state);
            st[0] = uvw[0];
            st[1] = uvw[1];
            // TODO: How do we get input texture coordinates derivatives ?
            dsdx = dsdy = dtdx = dtdy = 0.0;

            // A negative w value signals an "invalid" texture coordinate. This is
            // purely a convention.  An example usage would be where a tex coordinate
            // generator map (eg. camera projection) wants to signal to Image map that
            // the coordinate should be considered out of range, and thus we should
            // simply return black.
            if (uvw[2] < 0.f) {
                *sample = Vec3f(0.0f);
                return;
            }
        }
        break;
    default:
        st = state.getSt();
        break;
    }

    // If the "wrap around" parameter is false and we are outside the
    // 0 to 1 range, then just return the shading normal.
    if (!me->get(attrWrapAround) && isOutside(st)) {
        const Vec3f N = state.getN();
        *sample = Vec3f(N.x, N.y, N.z);
        return;
    }

    Color4 tx;
    if (me->mTexture) {
        // rotation and scaling only for non-udim case
        const Vec2f offset = me->get(attrOffset);
        const Vec2f scale  = me->get(attrScale);
        const Vec2f rotationCenter = me->get(attrRotationCenter);
        // Rotate coords and derivatives.
        float theta = deg2rad(me->get(attrRotationAngle));
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

        // sample the texture
        tx = me->mTexture->sample(tls, state, st, derivatives);
    } else if (me->mUdimTexture) {
        // compute udim index
        int udim = me->mUdimTexture->computeUdim(tls, st.x, st.y);
        if (udim == -1) {
            // mUdimTexture->computeUdim logs the invalid udim error
            *sample = Vec3f(0.0f);
            return;
        }

        // take fractional parts of st
        st.x = st.x - static_cast<int>(st.x);
        st.y = st.y - static_cast<int>(st.y);

        // Invert t coord.
        st.y = 1.0 - st.y;

        // Set derivatives.
        float derivatives[4] = { dsdx,
                                -dtdx,
                                 dsdy,
                                -dtdy };

        // sample the texture
        tx = me->mUdimTexture->sample(tls, state, udim, st, derivatives);
    }

    Vec3f normal(tx.r, tx.g, tx.b);

    // Validate normal
    if (std::isnan(normal.x) ||
        std::isnan(normal.y) ||
        std::isnan(normal.z) ||
        !std::isfinite(normal.x) ||
        !std::isfinite(normal.y) ||
        !std::isfinite(normal.z) ||
        isZero(normal)) {

        *sample = state.getN();
        return;
    }

    // Re-center sampled normal from [0, 1] --> [-1, 1]
    // z is usually encoded from [0.5, 1.0] so that this re-centering
    // keeps it positive (in [0, 1])
    const int normalMapEncoding = me->get(attrNormalEncoding);
    if (normalMapEncoding == 0) { // [0,1]
        normal = 2.0f * normal - Vec3f(1.0f, 1.0f, 1.0f);
    }

    // Transform from tangent space to shade space
    bool reversedNormals = false;
    if (state.isProvided(moonray::shading::StandardAttributes::sReversedNormals)) {
        reversedNormals = state.getAttribute(moonray::shading::StandardAttributes::sReversedNormals);
    }
    const scene_rdl2::math::Vec3f statedPds = reversedNormals ? state.getdPds() * -1.0f : state.getdPds() * 1.0f;
    const ReferenceFrame frame(state.getN(), scene_rdl2::math::normalize(statedPds));
    const Vec3f renderN = frame.localToGlobal(normal);

    // Don't normalize vector, need unnormalized length for
    // Toksvig normal anti-aliasing technique in Dwa materials.
    *sample = renderN;
}

