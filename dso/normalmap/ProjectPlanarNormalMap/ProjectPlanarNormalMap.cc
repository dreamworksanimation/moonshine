// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.cc"
#include "ProjectPlanarNormalMap_ispc_stubs.h"

#include <moonray/map/primvar/Primvar.h>
#include <moonshine/map/projection/TriplanarTexture.h>
#include <moonshine/map/projection/ProjectionUtil.h>

#include <moonray/common/mcrt_macros/moonray_static_check.h>
#include <moonray/rendering/shading/MapApi.h>
#include <scene_rdl2/render/util/stdmemory.h>

using namespace scene_rdl2::math;
using namespace moonshine;

namespace {
bool
isOutside(const Vec2f& uv)
{
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f) {
        return true;
    } else {
        return false;
    }
}
}

static ispc::PROJECTION_StaticData sStaticProjectPlanarNormalMapData;

//----------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(ProjectPlanarNormalMap, NormalMap)

public:
    ProjectPlanarNormalMap(const SceneClass& sceneClass, const std::string& name);
    ~ProjectPlanarNormalMap() override;
    void update() override;

private:
    static void sampleNormal(const NormalMap* self,
                             moonray::shading::TLState* tls,
                             const moonray::shading::State& state,
                             Vec3f* sample);
    ispc::ProjectPlanarNormalMap mIspc; // must be first member
    std::unique_ptr<moonray::shading::BasicTexture> mTexture;
    std::unique_ptr<moonray::shading::Xform> mProjectorXform;
    std::unique_ptr<moonray::shading::Xform> mObjXform;

RDL2_DSO_CLASS_END(ProjectPlanarNormalMap)

//----------------------------------------------------------------------------

ProjectPlanarNormalMap::ProjectPlanarNormalMap(const SceneClass& sceneClass, const std::string& name)
    : Parent(sceneClass, name)
{
    mSampleNormalFunc = ProjectPlanarNormalMap::sampleNormal;
    mSampleNormalFuncv = (SampleNormalFuncv) ispc::ProjectPlanarNormalMap_getSampleFunc();

    // Store keys to shader data
    mIspc.mRefPKey = moonray::shading::StandardAttributes::sRefP;
    mIspc.mRefNKey = moonray::shading::StandardAttributes::sRefN;

    mIspc.mStaticData = (ispc::PROJECTION_StaticData*)&sStaticProjectPlanarNormalMapData;

    const SceneVariables &sv = getSceneClass().getSceneContext()->getSceneVariables();
    asCpp(mIspc.mFatalColor) = sv.get(SceneVariables::sFatalColor);

    mTexture = fauxstd::make_unique<moonray::shading::BasicTexture>(this, mLogEventRegistry);
    mIspc.mTexture = &mTexture->getBasicTextureData();

    // Set projection error messages and fatal color
    projection::initLogEvents(*mIspc.mStaticData,
                              mLogEventRegistry,
                              this);
}

ProjectPlanarNormalMap::~ProjectPlanarNormalMap()
{
}

void
ProjectPlanarNormalMap::update()
{
    mIspc.mHasValidProjector = false;
    mIspc.mProjectorXform = nullptr;
    mProjectorXform = projection::getProjectorXform(this,
                                                    static_cast<ispc::PROJECTION_Mode>(get(attrProjectionMode)),
                                                    get(attrProjector),
                                                    get(attrProjectionMatrix),
                                                    static_cast<ispc::PROJECTION_TransformOrder>(get(attrTRSOrder)),
                                                    static_cast<ispc::PROJECTION_RotationOrder>(get(attrRotationOrder)),
                                                    get(attrTranslate),
                                                    get(attrRotate),
                                                    get(attrScale));
    if (mProjectorXform != nullptr) {
        mIspc.mProjectorXform = mProjectorXform->getIspcXform();
        mIspc.mHasValidProjector = true;
    }

    // Use reference space
    if (hasChanged(attrUseReferenceSpace)) {
        mRequiredAttributes.clear();
        mOptionalAttributes.clear();
        if (get(attrUseReferenceSpace)) {
            mRequiredAttributes.push_back(mIspc.mRefPKey);
        }
    }

    // Update texture map
    if (hasChanged(attrTexture) || hasChanged(attrWrapAround)) {
        std::string errorStr;

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
                              wrapS,                            // wrapS
                              wrapT,                            // wrapT
                              false,                            // use default color
                              sBlack,                           // default color
                              asCpp(mIspc.mFatalColor),         // fatal color
                              errorStr)) {
            fatal(errorStr);
        }
    }

    // Construct Xform for space of object being rendered to transform texture normals into
    mObjXform = fauxstd::make_unique<moonray::shading::Xform>(this, nullptr, nullptr, nullptr);
    mIspc.mObjXform = mObjXform->getIspcXform();

    // Note below we use a hard coded 0.5 value for the transition width.
    // In the triplanar projection maps the blending is between different
    // textures or different projections of the same texture so there it
    // is a user settable parameters.   Here it is only used to blend the
    // transformed normals of a single planar projected texture so a value
    // of 0.5 should be sufficient for all cases.
    mIspc.mTransitionWidthPower = projection::calculateTransitionWidthPower(0.5f);

    // Get whether or not the normals have been reversed
    mOptionalAttributes.push_back(moonray::shading::StandardAttributes::sReversedNormals);
    mIspc.mReversedNormalsIndx = moonray::shading::StandardAttributes::sReversedNormals.getIndex();
}

void
ProjectPlanarNormalMap::sampleNormal(const NormalMap* self,
                                     moonray::shading::TLState* tls,
                                     const moonray::shading::State& state,
                                     Vec3f* sample)
{
    const ProjectPlanarNormalMap* me = static_cast<const ProjectPlanarNormalMap*>(self);

    *sample = state.getN();

    if (!me->mIspc.mHasValidProjector) {
        // Log missing projector data message
        moonray::shading::logEvent(me, tls, me->mIspc.mStaticData->sErrorMissingProjector);
        return;
    }

    // Retrieve position and normal and transform into
    // the object space of the projector
    ispc::PRIMVAR_Input_Source_Mode inputSourceMode;
    if (me->get(attrUseReferenceSpace)) {
        inputSourceMode = ispc::INPUT_SOURCE_MODE_REF_P_REF_N;
    } else {
        inputSourceMode = ispc::INPUT_SOURCE_MODE_P_N;
    }

    Vec3f U, dUdx, dUdy, dUdz, inputPosition;
    if (!primvar::getPosition(tls, state,
                              inputSourceMode,
                              inputPosition,
                              me->mProjectorXform.get(),
                              ispc::SHADING_SPACE_OBJECT,
                              me->mIspc.mRefPKey,
                              U, dUdx, dUdy, dUdz)) {
        // Log missing ref_P data message
        moonray::shading::logEvent(me, tls, me->mIspc.mStaticData->sErrorMissingRefP);
        return;
    }

    Vec3f normal, inputNormal;
    if (!primvar::getNormal(tls, state,
                            inputSourceMode,
                            inputNormal,
                            me->mProjectorXform.get(),
                            ispc::SHADING_SPACE_OBJECT,
                            me->mIspc.mRefPKey,
                            me->mIspc.mRefNKey,
                            normal)) {
        // Log missing ref_N data message
        moonray::shading::logEvent(me, tls, me->mIspc.mStaticData->sErrorMissingRefN);
        return;
    }

    // Get 'U', our position w/ partial derivs as projected
    // onto the front cube face which is set as 2 below.
    projection::swizzleTriplanarFace(U, dUdx, dUdy, dUdz, 2,
                                     U, dUdx, dUdy, dUdz);

    // offset point so center is (0.5, 0.5)
    const Vec2f st(U.x + 0.5f, U.y + 0.5f);

    // If the "wrap around" parameter is false and we are outside the
    // 0 to 1 range, then just return the shading normal.
    if (!me->get(attrWrapAround) && isOutside(st)) {
        *sample = state.getN();
        return;
    }

    // Sample the tangent space normal texture using the swizzled derivatives
    float derivatives[4];
    derivatives[0] = dUdx.x; // dsdx
    derivatives[1] = dUdx.y; // dtdx
    derivatives[2] = dUdy.x; // dsdy
    derivatives[3] = dUdy.y; // dtdy
    Color tx = me->mTexture->sample(tls, state, st, derivatives);

    Vec3f tNormal =  Vec3f(tx.r, tx.g, tx.b);
    float tNormalLength = length(tNormal);

    // Remap the normals from the texture map (which, by default,
    // are assumed to be in the 0 to 1 range) to the -1 to 1 range.
    if (me->get(attrNormalEncoding) == 0) { // [0,1]
        tNormal = 2.0f * tNormal - Vec3f(scene_rdl2::math::one);
        tNormalLength = length(tNormal);
        tNormal = tNormal / tNormalLength; // normalize
    }

    // We flip the x component of the normal on three of the
    // sides to match the look of it's opposite side
    Vec3f tNormalFlippedX(-tNormal.x, tNormal.y, tNormal.z);

    // Blend the six object space normals based on the input normal direction
    Vec3f blending =
        projection::calculateTriplanarBlending(normal,
                                               me->mIspc.mTransitionWidthPower);

    Vec3f oNormal[6];
    for (size_t cubeface = 0; cubeface < 6; ++cubeface) {
        oNormal[cubeface] = normal;

        // projector object-space tangent
        Vec3f tangent;
        switch (cubeface) {
        case 0:
            tangent = Vec3f( 0.f,  0.f, -1.f);
            break;
        case 1:
            tangent = Vec3f( 1.f,  0.f,  0.f);
            break;
        case 2:
            tangent = Vec3f( 1.f,  0.f,  0.f);
            break;
        case 3:
            tangent = Vec3f( 0.f,  0.f,  1.f);
            break;
        case 4:
            tangent = Vec3f( 1.f,  0.f,  0.f);
            break;
        case 5:
            tangent = Vec3f(-1.f,  0.f,  0.f);
            break;
        }

        bool reversedNormals = false;
        if (state.isProvided(moonray::shading::StandardAttributes::sReversedNormals)) {
            reversedNormals = state.getAttribute(moonray::shading::StandardAttributes::sReversedNormals);
        }
        if (reversedNormals) {
            tangent *= -1.0f;
        }

        Vec3f crossP = cross(normal, tangent);
        if (isZero(crossP.x) && isZero(crossP.y) && isZero(crossP.z)) continue;

        // projector object-space ReferenceFrame
        ReferenceFrame frame(normal, tangent);

        // Use x flipped tNormal for +x, -y, and -z faces
        if (cubeface == 0 || cubeface == 4 || cubeface == 5) {
            oNormal[cubeface] = frame.localToGlobal(tNormalFlippedX);
        } else {
            oNormal[cubeface] = frame.localToGlobal(tNormal);
        }
    }

    // Blend directions
    const Vec3f bNormal = ((normal.x > 0.0f) ? oNormal[0] : oNormal[3]) * blending.x +
                          ((normal.y > 0.0f) ? oNormal[1] : oNormal[4]) * blending.y +
                          ((normal.z > 0.0f) ? oNormal[2] : oNormal[5]) * blending.z;

    Vec3f rNormal;
    if (me->get(attrUseReferenceSpace)) {
        // Transform blended normal from the object space of the
        // projector to world space
        const Vec3f pNormal = me->mProjectorXform->transformNormal(
                ispc::SHADING_SPACE_OBJECT,
                ispc::SHADING_SPACE_WORLD,
                state,
                bNormal);

        // Transform blended normal from world space to the
        // space of the rendered object and then to render space
        rNormal = me->mObjXform->transformNormal(
            ispc::SHADING_SPACE_OBJECT,
            ispc::SHADING_SPACE_RENDER,
            state,
            pNormal);
    } else {
        // Transform blended normal from the object space of the
        // projector to render space
        rNormal = me->mProjectorXform->transformNormal(
            ispc::SHADING_SPACE_OBJECT,
            ispc::SHADING_SPACE_RENDER,
            state,
            bNormal);
    }

    // bNormalLength is used to preserve the original length of the normal,
    // which is necessary for the Toksvig mapping technique which recalculates
    // the roughness value based on the length of the input normal in the Dwa materials.
    // adjust the normal as necessary with to avoid black
    // artifacts at oblique viewing angles.
    const float bNormalLength = tNormalLength * (blending.x + blending.y + blending.z);
    *sample = bNormalLength * normalize(rNormal);
}

