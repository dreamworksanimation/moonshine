// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.cc"
#include "RampMap_ispc_stubs.h"

#include <moonray/common/mcrt_macros/moonray_static_check.h>
#include <moonray/rendering/shading/MapApi.h>
#include <moonray/rendering/shading/RampControl.h>

#include <memory>

using namespace scene_rdl2::math;

//----------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(RampMap, Map)
public:
    RampMap(const SceneClass& sceneClass, const std::string& name);
    ~RampMap() override;
    void update() override;

private:
    static void sample(const Map* self, moonray::shading::TLState* tls,
                       const moonray::shading::State& state, Color* sample);

    Color evaluateRamp(const Vec3f& pos, const Map* self, moonray::shading::TLState* tls,
                             const moonray::shading::State& state) const;

    bool validateRampInputs();

    ispc::RampMap mIspc; // must be first member
    std::unique_ptr<moonray::shading::Xform> mXform;
    moonray::shading::ColorRampControl mRampControl;


RDL2_DSO_CLASS_END(RampMap)

//----------------------------------------------------------------------------

namespace {

// Manipulate uvs to produce a resulting wave pattern
// Input uvs are modified
void
applyWave(const Vec2f& uvWave, Vec2f& uv)
{
    if (uvWave.x > 0.0f) {
        uv.x += scene_rdl2::math::sin(2.0f * sPi * uv.y) * uvWave.x;
    }
    if (uvWave.y > 0.0f) {
        uv.y += scene_rdl2::math::sin(2.0f * sPi * uv.x) * uvWave.y;
    }
}

// Wrap or clamp the uv based on user input
void
applyWrap(const int wrap, Vec2f& uv)
{
    if (wrap == ispc::WRAP) {
        if (uv.x > 1.0f || uv.x < 0.0f) {
            uv.x -= scene_rdl2::math::floor(uv.x);
        }
        if (uv.y > 1.0f || uv.y < 0.0f) {
            uv.y -= scene_rdl2::math::floor(uv.y);
        }
    } else { // Clamp
        uv.x = saturate(uv.x);
        uv.y = saturate(uv.y);
    }
}
} // namespace

RampMap::RampMap(const SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name),
    mRampControl()
{
    mSampleFunc = RampMap::sample;
    mSampleFuncv = (SampleFuncv) ispc::RampMap_getSampleFunc();

    mIspc.mRefPKey = moonray::shading::StandardAttributes::sRefP;
}

RampMap::~RampMap()
{
}

bool
RampMap::validateRampInputs()
{

    if (get(attrRampType) > 8 || get(attrRampType) < 0) {
        fatal("Ramp: Unsupported ramp type");
        return false;
    }
    if (get(attrWrapType) > 1 || get(attrWrapType) < 0) {
        fatal("Ramp: Unsupported wrap type");
        return false;
    }
    if (get(attrSpace) > 6 || get(attrSpace) < 0) {
        fatal("Ramp: Unsupported space");
        return false;
    }
    if (get(attrColorSpace) >= ispc::COLOR_RAMP_CONTROL_SPACE_TOTAL || get(attrColorSpace) < 0) {
        fatal("Ramp: Unsupported color space");
        return false;
    }

    // Validate input object and camera, if used in the shader
    if (get(attrSpace) == ispc::SHADING_SPACE_OBJECT) {
        const Geometry* geom = get(attrObject) ?
                get(attrObject)->asA<Geometry>() : nullptr;
        if (geom == nullptr) {
            warn("Ramp: Input object not provided for object space transforms. Defaulting to "
                    "shading point's object space");
        }
    }

    if (get(attrSpace) == ispc::SHADING_SPACE_CAMERA) {
        const Camera* cam = get(attrCamera) ?
                get(attrCamera)->asA<Camera>() : nullptr;
        if (cam == nullptr) {
            warn("Ramp: Input camera not provided for camera space transforms. Defaulting to "
                    "scene's current active camera");
        }
    }

    // Get the ramp data points and validate them
    const std::vector<float>& positions = get(attrPositions);
    const std::vector<Color>& colors = get(attrColors);
    const std::vector<int>& interpolations = get(attrInterpolations);

    for (auto&& interp : interpolations) {
        if (interp > ispc::RAMP_INTERPOLATOR_MODE_MONOTONECUBIC) {
            fatal("Ramp: Unsupported interpolation mode");
            return false;
        }
    }

    // Number of ramp inputs are capped since ISPC needs statically allocated arrays
    uint32_t numRampCVs = positions.size();
    if (numRampCVs > ispc::RAMPMAP_MAX_DATASIZE) {
        fatal("Please provide less than ", ispc::RAMPMAP_MAX_DATASIZE, " ramp data points.");
        return false;
    }

    // Ensure that there are equal number of positions, colors and interpolations in user input
    if (positions.size() != colors.size() || positions.size() != interpolations.size()) {
        fatal("Ramp positions, colors and interpolations are not of same size");
        return false;
    }

    if (get(attrRampType) == ispc::RAMP_INTERPOLATOR_2D_TYPE_FOUR_CORNER_RAMP && positions.size() != 4) {
        fatal("Please provide only 4 ramp points for Four Corner Ramp");
        return false;
    }

    return true;
}

void
RampMap::update()
{
    if (!validateRampInputs()) {
        fatal("Ramp input validation failed");
        return;
    }

    // Set fatal color
    const SceneVariables &sv = getSceneClass().getSceneContext()->getSceneVariables();
    mIspc.mFatalColor = asIspc(sv.get(SceneVariables::sFatalColor));

    // Get input object and camera
    const Geometry* geom = get(attrObject) ?
                get(attrObject)->asA<Node>() : nullptr;
    const Camera* cam = get(attrCamera) ?
            get(attrCamera)->asA<Camera>() : nullptr;

    // Construct Xform with custom camera
    mXform = std::make_unique<moonray::shading::Xform>(this, geom, cam, nullptr);
    mIspc.mXform = mXform->getIspcXform();

    mIspc.mRampControl = mRampControl.asIspc();

    // If using reference space, request reference space
    // primitive attributes from the geometry
    if (hasChanged(attrSpace)) {
        mRequiredAttributes.clear();
        mOptionalAttributes.clear();
        if (get(attrSpace) == ispc::SHADING_SPACE_REFERENCE) {
            mRequiredAttributes.push_back(mIspc.mRefPKey);
        }
    }

    const std::vector<float>& positions = get(attrPositions);
    const std::vector<Color>& colors = get(attrColors);
    const std::vector<int>& interpolations = get(attrInterpolations);
    const int colorSpace = get(attrColorSpace);

    mRampControl.init(positions.size(),
                      positions.data(),
                      colors.data(),
                      reinterpret_cast<const ispc::RampInterpolatorMode*>(interpolations.data()),
                      static_cast<ispc::ColorRampControlSpace>(colorSpace),
                      false); // applyHueBlendAdjustment
}

Color
RampMap::evaluateRamp(const Vec3f& pos, const Map* self, moonray::shading::TLState* tls,
                      const moonray::shading::State& state) const
{
    // The ramp is only evaluated in X or Y or both, Z
    // of the position is ignored.
    Vec2f uv(pos.x, pos.y);

    // Apply uv repeat; no repeats happen with default value of (1.0, 1.0)
    uv = uv * get(attrUvRepeat);

    // Apply wave in u and v
    const Vec2f uvWave = get(attrUvWave);
    applyWave(uvWave, uv);

    // Handle wrap types
    const int wrap = get(attrWrapType);
    applyWrap(wrap, uv);

    // Evaluate ramp result based on type of ramp and interpolation
    const ispc::RampInterpolator2DType rampType = (ispc::RampInterpolator2DType)get(attrRampType);
    Color result = mRampControl.eval2D(uv, rampType, evalFloat(self, attrInput, tls, state));
    return result;
}


void
RampMap::sample(const Map* self, moonray::shading::TLState* tls,
                const moonray::shading::State& state, Color* sample)
{
    const RampMap* me = static_cast<const RampMap*>(self);

    Vec3f pos;

    if (me->get(attrTextureEnum) == 1) {
        // Use alternate UVs.
        Vec3f uvw = evalVec3f(me, attrInputTextureCoordinate, tls, state);
        pos = Vec3f(uvw.x, uvw.y, 0.f);
    } else {
        // Use state UVs.
        // Retrieve position
        const int space = me->get(attrSpace);
        if (space == ispc::SHADING_SPACE_TEXTURE) {
            pos = Vec3f(
                state.getSt().x,
                state.getSt().y,
                0.0f
            );
        } else if (space == ispc::SHADING_SPACE_REFERENCE) {
            state.getRefP(pos);
        } else {
            pos = state.getP();
            if (space != ispc::SHADING_SPACE_RENDER) {
                pos = me->mXform->transformPoint(ispc::SHADING_SPACE_RENDER,
                                                 space,
                                                 state,
                                                 pos);
            }
        }
    }

    *sample = me->evaluateRamp(pos, self, tls, state);
}

