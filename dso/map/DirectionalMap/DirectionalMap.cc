// Copyright 2023 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0


#include "attributes.cc"
#include "DirectionalMap_ispc_stubs.h"

#include <moonshine/common/interpolation/Interpolation.h>
#include <moonray/common/mcrt_macros/moonray_static_check.h>
#include <moonray/rendering/shading/MapApi.h>
#include <scene_rdl2/render/util/stdmemory.h>

using namespace moonshine;
using namespace scene_rdl2::math;

static ispc::DirectionalMapStaticData sDirectionalMapData;

//---------------------------------------------------------------------------

RDL2_DSO_CLASS_BEGIN(DirectionalMap, Map)

public:
    DirectionalMap(const SceneClass& sceneClass, const std::string& name);
    ~DirectionalMap() override;
    void update() override;

private:
    static void sample(const Map* self, moonray::shading::TLState *tls,
                       const moonray::shading::State& state, Color* sample);

    Vec3f computeObjectAxis(const int axis,
                                  const moonray::shading::State &state) const;

    ispc::DirectionalMap mIspc;
    NormalMap* mNormalMap;
    std::unique_ptr<moonray::shading::Xform> mXform;

RDL2_DSO_CLASS_END(DirectionalMap)


//---------------------------------------------------------------------------

DirectionalMap::DirectionalMap(const SceneClass& sceneClass, const std::string& name) :
    Parent(sceneClass, name)
{
    mSampleFunc = DirectionalMap::sample;
    mSampleFuncv =   (SampleFuncv)   ispc::DirectionalMap_getSampleFunc();

    mIspc.mRefPKey = moonray::shading::StandardAttributes::sRefP;
    mIspc.mRefNKey = moonray::shading::StandardAttributes::sRefN;

    mIspc.mStaticData = (ispc::DirectionalMapStaticData*)&sDirectionalMapData;

    // register shade time event messages.  we require and expect
    // these events to have the same value across all instances of the shader.
    // no conditional registration of events is allowed.
    // to allow for the possibility that we may someday create image maps
    // on multiple threads, we'll protect the writes of the class statics
    // with a mutex.
    static std::mutex errorMutex;
    std::scoped_lock lock(errorMutex);
    MOONRAY_START_THREADSAFE_STATIC_WRITE

    sDirectionalMapData.sErrorMissingRefN = mLogEventRegistry.createEvent(scene_rdl2::logging::ERROR_LEVEL,
        "ref_N primitive attribute is missing and cannot be computed from ref_P partials");

    MOONRAY_FINISH_THREADSAFE_STATIC_WRITE
}

DirectionalMap::~DirectionalMap()
{
}

void
DirectionalMap::update()
{
    const Node* node = get(attrObject) ?
        get(attrObject)->asA<Node>() : nullptr;

    mXform = fauxstd::make_unique<moonray::shading::Xform>(this, node, nullptr, nullptr);
    mIspc.mXform = mXform->getIspcXform();

    // Use reference space
    if (hasChanged(attrUseReferenceSpace)) {
        mRequiredAttributes.clear();
        mOptionalAttributes.clear();
        if (get(attrUseReferenceSpace)) {
            mRequiredAttributes.push_back(mIspc.mRefPKey);
            mOptionalAttributes.push_back(mIspc.mRefNKey);
        }
    }

    mNormalMap = get(attrInputNormal) ?
                 get(attrInputNormal)->asA<NormalMap>() :
                 nullptr;

    mIspc.mNormalMap = mNormalMap;

    mIspc.mSampleNormalFunc = (mNormalMap != nullptr) ?
                              (intptr_t) mNormalMap->mSampleNormalFuncv :
                              (intptr_t) nullptr;
}

Vec3f
DirectionalMap::computeObjectAxis(const int axis,
                                  const moonray::shading::State &state) const
{
    static const Vec3f axes[6] = {
        Vec3f( 1.0f,  0.0f,  0.0f),
        Vec3f(-1.0f,  0.0f,  0.0f),
        Vec3f( 0.0f,  1.0f,  0.0f),
        Vec3f( 0.0f, -1.0f,  0.0f),
        Vec3f( 0.0f,  0.0f,  1.0f),
        Vec3f( 0.0f,  0.0f, -1.0f)
    };

    Vec3f dir = mXform->transformVector(ispc::SHADING_SPACE_OBJECT,
                                              ispc::SHADING_SPACE_RENDER,
                                              state,
                                              axes[axis]);
    return normalize(dir);
}

void
DirectionalMap::sample(const Map* self, moonray::shading::TLState *tls,
                 const moonray::shading::State& state, Color* sample)
{
    const DirectionalMap* me = static_cast<const DirectionalMap*>(self);

    // determine our 'prime' direction
    Vec3f prime;
    Vec3f P = state.getP();
    const int dirSource = me->get(attrPrimeDirection);

    switch (dirSource) {
        case ispc::DIRECTIONAL_PRIME_SOURCE_AXIS :
            prime = me->computeObjectAxis(me->get(attrObjectAxis), state);
            break;

        case ispc::DIRECTIONAL_PRIME_SOURCE_LOCATION :
            if (me->get(attrUseReferenceSpace)) {
                state.getRefP(P);
                prime = P; // refP is already in object space
            } else {
                // transform our shading point into the local space of our object
                prime = me->mXform->transformPoint(ispc::SHADING_SPACE_RENDER,
                                                   ispc::SHADING_SPACE_OBJECT,
                                                   state,
                                                   P);
            }

            // treat our point as a vector and flip it so that it points
            // from the shading position (or reference position) to the
            // object, and transform that vector back into render space
            prime = me->mXform->transformVector(ispc::SHADING_SPACE_OBJECT,
                                                ispc::SHADING_SPACE_RENDER,
                                                state,
                                                -prime);

            if (lengthSqr(prime) > 0.0f) {
                prime = normalize(prime);
            }
            break;

        case ispc::DIRECTIONAL_PRIME_SOURCE_OBSERVER :
            prime = state.getWo();
            break;

        case ispc::DIRECTIONAL_PRIME_SOURCE_CUSTOM :
        default :
            prime = evalVec3f(self, attrCustomDirection, tls, state);
            prime = me->mXform->transformVector(ispc::SHADING_SPACE_WORLD,
                                                ispc::SHADING_SPACE_RENDER,
                                                state,
                                                prime);

            if (lengthSqr(prime) > 0.0f) {
                prime = normalize(prime);
            }
            break;
    } // end switch

    // get N
    Vec3f N;
    if (me->get(attrUseReferenceSpace)) {
        if (!state.getRefN(N)) {
            // There really isn't a right thing to do here, its too late
            // to fatal() the shader.
            moonray::shading::logEvent(me, tls, sDirectionalMapData.sErrorMissingRefN);
            N = Vec3f(0.0f, 1.0f, 0.0f);
        }

        // TODO: this ignores normal mapping.  Do we have a way to
        // compute the perturbed normal in reference space ?

        // transform our normal into render space
        N = me->mXform->transformNormal(ispc::SHADING_SPACE_OBJECT,
                                        ispc::SHADING_SPACE_RENDER,
                                        state,
                                        N);

    } else {
        N = moonray::shading::evalNormal(me,
                                         attrInputNormal,
                                         evalFloat(self, attrInputNormalDial, tls, state),
                                         tls, state);
    }

    float cosTheta = dot(N, prime);
    if (me->get(attrClamping) == ispc::DIRECTIONAL_BEHAVIOR_ABS) {
        cosTheta = scene_rdl2::math::abs(cosTheta);
    }
    cosTheta = saturate(cosTheta);

    if (me->get(attrPolarity) == ispc::DIRECTIONAL_POLARITY_PERPENDICULAR) {
        cosTheta = 1.0f - cosTheta;
    }

    const int falloffType = me->get(attrFalloffType);
    float falloff;
    if (falloffType == ispc::DIRECTIONAL_FALLOFF_COSINE) {
        falloff = cosTheta;
    } else {    // ispc::DIRECTIONAL_FALLOFF_LINEAR
        falloff = 1.0f - scene_rdl2::math::acos(cosTheta) / sHalfPi;
    }

    const float biasVal = saturate(evalFloat(me, attrBias, tls, state));
    falloff = bias(falloff, biasVal);

    if (me->get(attrUseSmoothstep)) {
        falloff = interpolation::smoothStep(falloff,
                                            evalFloat(me, attrSmoothStart, tls, state),
                                            evalFloat(me, attrSmoothEnd, tls, state));
    }

    const Color clrA = evalColor(me, attrColorA, tls, state);
    const Color clrB = evalColor(me, attrColorB, tls, state);
    const Color result = lerp(clrA, clrB, falloff);

    *sample = result;
}


//---------------------------------------------------------------------------

