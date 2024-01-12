// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

///

#include "PrimitiveUserData.h"

using namespace moonray;
using namespace moonray::geom;
using namespace moonray::shading;

namespace moonshine {
namespace geometry {

#define INVALID_MESH_PART_INDEX -1

template <class T>
static void
addPrimitiveAttribute(shading::PrimitiveAttributeTable& primitiveAttributeTable,
                      const scene_rdl2::rdl2::String& key,
                      const std::vector<int>& facesetPartIndices,
                      int meshPartIndex,
                      std::vector<T>&& data)
{
    // Only constant and part rates are currently supported.  Higher rates will
    //  need additional attributes on the ABC object, such as the number of attribute elements
    //  per part... as the attribute data is just one big list and we would need to
    //  know which block of values was for each part.
    if (data.size() == 1) {
        // One value is specified for the entire ABC geom.  Assume a constant rate on the mesh.
        //  Just assign the value to the attribute.  We don't care about the facesets or parts
        //  specified on the ABC "part names" attr.
        primitiveAttributeTable.addAttribute(shading::TypedAttributeKey<T>(key),
                                             shading::AttributeRate::RATE_CONSTANT,
                                             std::move(data));
    } else {
        // Multiple values are specified, so assume one value per part specified in the ABC's
        //  "part names" attr.
        //  But... we need to pick out just the values that pertain to this mesh's facesets,
        //  and then assign those to the attribute.  We use the faceset's part index to pick
        //  out the value from the list of values.  Be careful not to overrun the values data buffer
        //  if it is too short.  If data is missing (the face set is not in the part list and
        //  hence there is no attribute data, or the attribute data is too short), use a default
        //  value.
        //  Note that facesetPartIndices.size() == number of facesets on the current mesh
        std::vector<T> perPartData(facesetPartIndices.size(), T());  // fill with default value for the type T
        for (size_t i = 0; i < facesetPartIndices.size(); i++) {
            // in part list and data is available?
            if (facesetPartIndices[i] != -1 && (size_t)facesetPartIndices[i] < data.size()) {
                perPartData[i] = data[facesetPartIndices[i]];
            } // else keep the default value
        }

        // Figure out the last value in the perPartData, which is the value for the
        // overall mesh.
        if (meshPartIndex != INVALID_MESH_PART_INDEX) {
            // The mesh was specified in the global part list and thus has an index
            //  into the attribute data array.
            if (meshPartIndex < (int)data.size()) {
                perPartData.push_back(data[meshPartIndex]);
            } else {
                // The index is out of bounds of the data array, just use a default value.
                perPartData.push_back(T());
            }
        } else {
            // The mesh is not specified in the global part list.  Use a default value
            // for the mesh.  This is used for faces that don't belong to any face sets.
            perPartData.push_back(T());
        }

        if (perPartData.size() == 1) {
            primitiveAttributeTable.addAttribute(shading::TypedAttributeKey<T>(key),
                                                 shading::AttributeRate::RATE_CONSTANT,
                                                 std::move(perPartData));
        } else {
            primitiveAttributeTable.addAttribute(shading::TypedAttributeKey<T>(key),
                                                 shading::AttributeRate::RATE_PART,
                                                 std::move(perPartData));
        }
    }
}

template <class T>
static void
addPrimitiveAttribute(shading::PrimitiveAttributeTable& primitiveAttributeTable,
                      const scene_rdl2::rdl2::String& key,
                      const std::vector<int>& facesetPartIndices,
                      int meshPartIndex,
                      std::vector<std::vector<T>>&& data)
{
    // Only constant and part rates are currently supported.  Higher rates will
    //  need additional attributes on the ABC object, such as the number of attribute elements
    //  per part... as the attribute data is just one big list and we would need to
    //  know which block of values was for each part.
    if (data[0].size() == 1) {
        // One value is specified for the entire ABC geom.  Assume a constant rate on the mesh.
        //  Just assign the value to the attribute.  We don't care about the facesets or parts
        //  specified on the ABC "part names" attr.
        primitiveAttributeTable.addAttribute(shading::TypedAttributeKey<T>(key),
                                             shading::AttributeRate::RATE_CONSTANT,
                                             std::move(data));
    } else {
        // Multiple values are specified, so assume one value per part specified in the ABC's
        //  "part names" attr.
        //  But... we need to pick out just the values that pertain to this mesh's facesets,
        //  and then assign those to the attribute.  We use the faceset's part index to pick
        //  out the value from the list of values.  Be careful not to overrun the values data buffer
        //  if it is too short.  If data is missing (the face set is not in the part list and
        //  hence there is no attribute data, or the attribute data is too short), use a default
        //  value.
        //  Note that facesetPartIndices.size() == number of facesets on the current mesh

        // fill with default value for the type T
        std::vector<std::vector<T>> perPartData(data.size(), std::vector<T>(facesetPartIndices.size(), T()));
        for (size_t t = 0; t < data.size(); ++t) {
            for (size_t i = 0; i < facesetPartIndices.size(); i++) {
                // in part list and data is available?
                if (facesetPartIndices[i] != -1 && (size_t)facesetPartIndices[i] < data[t].size()) {
                    perPartData[t][i] = data[t][facesetPartIndices[i]];
                } // else keep the default value
            }

            // Figure out the last value in the perPartData, which is the value for the
            // overall mesh.
            if (meshPartIndex != INVALID_MESH_PART_INDEX) {
                // The mesh was specified in the global part list and thus has an index
                //  into the attribute data array.
                if (meshPartIndex < (int)data[t].size()) {
                    if (facesetPartIndices.size() == 1 && facesetPartIndices.front() == -1) {
                        // The mesh has no facesets in the part list
                        perPartData[t].front() = data[t][meshPartIndex];
                    } else {
                        perPartData[t].push_back(data[t][meshPartIndex]);
                    }
                } else {
                    // The index is out of bounds of the data array, just use a default value.
                    perPartData[t].push_back(T());
                }
            } else {
                // The mesh is not specified in the global part list.  Use a default value
                // for the mesh.  This is used for faces that don't belong to any face sets.
                perPartData[t].push_back(T());
            }
        }

        if (perPartData[0].size() == 1) {
            primitiveAttributeTable.addAttribute(shading::TypedAttributeKey<T>(key),
                                                 shading::AttributeRate::RATE_CONSTANT,
                                                 std::move(perPartData));
        } else {
            primitiveAttributeTable.addAttribute(shading::TypedAttributeKey<T>(key),
                                                 shading::AttributeRate::RATE_PART,
                                                 std::move(perPartData));
        }
    }
}


void
processMeshUserData(const scene_rdl2::rdl2::SceneObjectVector& arbitraryData,
                    const std::vector<int>& partIndices,
                    int meshPartIndex,
                    bool useFirstFrame, bool useSecondFrame,
                    shading::PrimitiveAttributeTable& primitiveAttributeTable)
{
    // Similar to the RdlMesh code

    for (auto sceneObject : arbitraryData) {
        const scene_rdl2::rdl2::UserData* userData = sceneObject->asA<scene_rdl2::rdl2::UserData>();
        if (!userData) {
            continue;
        }

        if (userData->hasBoolData()) {
            const scene_rdl2::rdl2::String& key = userData->getBoolKey();
            // bool vector is a std::deque
            const scene_rdl2::rdl2::BoolVector& constData = userData->getBoolValues();
            std::vector<scene_rdl2::rdl2::Bool> data(constData.begin(), constData.end());
            addPrimitiveAttribute(primitiveAttributeTable, key, partIndices, meshPartIndex, std::move(data));
        }

        if (userData->hasIntData()) {
            const scene_rdl2::rdl2::String& key = userData->getIntKey();
            scene_rdl2::rdl2::IntVector data = userData->getIntValues();
            addPrimitiveAttribute(primitiveAttributeTable, key, partIndices, meshPartIndex, std::move(data));
        }

        std::vector<scene_rdl2::rdl2::FloatVector> floatData;
        if (userData->hasFloatData0() && useFirstFrame) {
            const scene_rdl2::rdl2::FloatVector& data0 = userData->getFloatValues0();
            floatData.push_back(data0);
        }
        if (userData->hasFloatData1() && useSecondFrame) {
            const scene_rdl2::rdl2::FloatVector& data1 = userData->getFloatValues1();
            floatData.push_back(data1);
        }
        if (!floatData.empty()) {
            const scene_rdl2::rdl2::String& key = userData->getFloatKey();
            addPrimitiveAttribute(primitiveAttributeTable, key, partIndices, meshPartIndex, std::move(floatData));
        }

        if (userData->hasStringData()) {
            const scene_rdl2::rdl2::String& key = userData->getStringKey();
            scene_rdl2::rdl2::StringVector data = userData->getStringValues();
            addPrimitiveAttribute(primitiveAttributeTable, key, partIndices, meshPartIndex, std::move(data));
        }

        std::vector<scene_rdl2::rdl2::RgbVector> colorData;
        if (userData->hasColorData0() && useFirstFrame) {
            const scene_rdl2::rdl2::RgbVector& data0 = userData->getColorValues0();
            colorData.push_back(data0);
        }
        if (userData->hasColorData1() && useSecondFrame) {
            const scene_rdl2::rdl2::RgbVector& data1 = userData->getColorValues1();
            colorData.push_back(data1);
        }
        if (!colorData.empty()) {
            const scene_rdl2::rdl2::String& key = userData->getColorKey();
            addPrimitiveAttribute(primitiveAttributeTable, key, partIndices, meshPartIndex, std::move(colorData));
        }

        std::vector<scene_rdl2::rdl2::Vec2fVector> vec2fData;
        if (userData->hasVec2fData0() && useFirstFrame) {
            const scene_rdl2::rdl2::Vec2fVector& data0 = userData->getVec2fValues();
            vec2fData.push_back(data0);
        }
        if (userData->hasVec2fData1() && useSecondFrame) {
            const scene_rdl2::rdl2::Vec2fVector& data1 = userData->getVec2fValues1();
            vec2fData.push_back(data1);
        }
        if (!vec2fData.empty()) {
            const scene_rdl2::rdl2::String& key = userData->getVec2fKey();
            addPrimitiveAttribute(primitiveAttributeTable, key, partIndices, meshPartIndex, std::move(vec2fData));
        }

        std::vector<scene_rdl2::rdl2::Vec3fVector> vec3fData;
        if (userData->hasVec3fData0() && useFirstFrame) {
            const scene_rdl2::rdl2::Vec3fVector& data0 = userData->getVec3fValues();
            vec3fData.push_back(data0);
        }
        if (userData->hasVec3fData1() && useSecondFrame) {
            const scene_rdl2::rdl2::Vec3fVector& data1 = userData->getVec3fValues1();
            vec3fData.push_back(data1);
        }
        if (!vec3fData.empty()) {
            const scene_rdl2::rdl2::String& key = userData->getVec3fKey();
            addPrimitiveAttribute(primitiveAttributeTable, key, partIndices, meshPartIndex, std::move(vec3fData));
        }

        std::vector<scene_rdl2::rdl2::Mat4fVector> mat4fData;
        if (userData->hasMat4fData0() && useFirstFrame) {
            const scene_rdl2::rdl2::Mat4fVector& data0 = userData->getMat4fValues();
            mat4fData.push_back(data0);
        }
        if (userData->hasMat4fData1() && useSecondFrame) {
            const scene_rdl2::rdl2::Mat4fVector& data1 = userData->getMat4fValues1();
            mat4fData.push_back(data1);
        }
        if (!mat4fData.empty()) {
            const scene_rdl2::rdl2::String& key = userData->getMat4fKey();
            addPrimitiveAttribute(primitiveAttributeTable, key, partIndices, meshPartIndex, std::move(mat4fData));
        }
    }
}

template <class T>
static void
addCurvesPrimitiveAttribute(shading::PrimitiveAttributeTable& primitiveAttributeTable,
                            const scene_rdl2::rdl2::String& key,
                            const std::vector<std::string> * const partList,
                            const std::string& partName,
                            std::vector<T>&& data)
{
    if (data.size() == 1) {
        // One value is specified for all of the curves.
        primitiveAttributeTable.addAttribute(TypedAttributeKey<T>(key),
                                             shading::AttributeRate::RATE_CONSTANT,
                                             std::move(data));

    } else if (partList != nullptr && partName != "" && partList->size() == data.size()) {
        // If the size of the part list matches the size of the data, try
        // to find the partName in the partList.   If found, add the data
        // at the partName's index at constant rate.
        for (size_t j = 0; j < partList->size(); ++j) {
            if (partName == (*partList)[j]) {
                primitiveAttributeTable.addAttribute(TypedAttributeKey<T>(key),
                                                     shading::AttributeRate::RATE_CONSTANT,
                                                     std::move(std::vector<T>(1, data[j])));
            }
        }

    } else {
        // One value is specified for each curve
        primitiveAttributeTable.addAttribute(TypedAttributeKey<T>(key),
                                             shading::AttributeRate::RATE_UNIFORM,
                                             std::move(data));
    }
}

template <class T>
static void
addCurvesPrimitiveAttribute(shading::PrimitiveAttributeTable& primitiveAttributeTable,
                            const scene_rdl2::rdl2::String& key,
                            const std::vector<std::string> * const partList,
                            const std::string& partName,
                            std::vector<std::vector<T>>&& data)
{
    if (data[0].size() == 1) {
        // One value is specified for all of the curves.
        primitiveAttributeTable.addAttribute(TypedAttributeKey<T>(key),
                                             shading::AttributeRate::RATE_CONSTANT,
                                             std::move(data));

    } else if (partList != nullptr && partName != "" && partList->size() == data[0].size()) {
        // If the size of the part list matches the size of the data, try
        // to find the partName in the partList.   If found, add the data
        // at the partName's index at constant rate.
        for (size_t j = 0; j < partList->size(); ++j) {
            if (partName == (*partList)[j]) {
                std::vector<std::vector<T>> perPartData(data.size(), std::vector<T>(1, T()));
                for (size_t t = 0; t < data.size(); t++) {
                    perPartData[t][0] = data[t][j];
                }
                primitiveAttributeTable.addAttribute(TypedAttributeKey<T>(key),
                                                     shading::AttributeRate::RATE_CONSTANT,
                                                     std::move(perPartData));
            }
        }

    } else {
        // One value is specified for each curve
        primitiveAttributeTable.addAttribute(TypedAttributeKey<T>(key),
                                             shading::AttributeRate::RATE_UNIFORM,
                                             std::move(data));
    }
}

void
processUserData(const scene_rdl2::rdl2::SceneObjectVector& arbitraryData,
                const std::vector<std::string> * const partList,
                const std::string& partName,
                bool useFirstFrame, bool useSecondFrame,
                shading::PrimitiveAttributeTable& primitiveAttributeTable)
{
    for (auto sceneObject : arbitraryData) {
        const scene_rdl2::rdl2::UserData* userData = sceneObject->asA<scene_rdl2::rdl2::UserData>();
        if (!userData) {
            continue;
        }

        if (userData->hasBoolData()) {
            const scene_rdl2::rdl2::String& key = userData->getBoolKey();
            // bool vector is a std::deque
            const scene_rdl2::rdl2::BoolVector& constData = userData->getBoolValues();
            std::vector<scene_rdl2::rdl2::Bool> data(constData.begin(), constData.end());
            addCurvesPrimitiveAttribute(primitiveAttributeTable, key, partList, partName, std::move(data));
        }

        if (userData->hasIntData()) {
            const scene_rdl2::rdl2::String& key = userData->getIntKey();
            scene_rdl2::rdl2::IntVector data = userData->getIntValues();
            addCurvesPrimitiveAttribute(primitiveAttributeTable, key, partList, partName, std::move(data));
        }

        std::vector<scene_rdl2::rdl2::FloatVector> floatData;
        if (userData->hasFloatData0() && useFirstFrame) {
            const scene_rdl2::rdl2::FloatVector& data0 = userData->getFloatValues0();
            floatData.push_back(data0);
        }
        if (userData->hasFloatData1() && useSecondFrame) {
            const scene_rdl2::rdl2::FloatVector& data1 = userData->getFloatValues1();
            floatData.push_back(data1);
        }
        if (!floatData.empty()) {
            const scene_rdl2::rdl2::String& key = userData->getFloatKey();
            addCurvesPrimitiveAttribute(primitiveAttributeTable, key, partList, partName, std::move(floatData));
        }

        if (userData->hasStringData()) {
            const scene_rdl2::rdl2::String& key = userData->getStringKey();
            scene_rdl2::rdl2::StringVector data = userData->getStringValues();
            addCurvesPrimitiveAttribute(primitiveAttributeTable, key, partList, partName, std::move(data));
        }

        std::vector<scene_rdl2::rdl2::RgbVector> colorData;
        if (userData->hasColorData0() && useFirstFrame) {
            const scene_rdl2::rdl2::RgbVector& data0 = userData->getColorValues0();
            colorData.push_back(data0);
        }
        if (userData->hasColorData1()) {
            const scene_rdl2::rdl2::RgbVector& data1 = userData->getColorValues1();
            colorData.push_back(data1);
        }
        if (!colorData.empty()) {
            const scene_rdl2::rdl2::String& key = userData->getColorKey();
            addCurvesPrimitiveAttribute(primitiveAttributeTable, key, partList, partName, std::move(colorData));
        }

        std::vector<scene_rdl2::rdl2::Vec2fVector> vec2fData;
        if (userData->hasVec2fData0() && useFirstFrame) {
            const scene_rdl2::rdl2::Vec2fVector& data0 = userData->getVec2fValues();
            vec2fData.push_back(data0);
        }
        if (userData->hasVec2fData1() && useSecondFrame) {
            const scene_rdl2::rdl2::Vec2fVector& data1 = userData->getVec2fValues1();
            vec2fData.push_back(data1);
        }
        if (!vec2fData.empty()) {
            const scene_rdl2::rdl2::String& key = userData->getVec2fKey();
            addCurvesPrimitiveAttribute(primitiveAttributeTable, key, partList, partName, std::move(vec2fData));
        }

        std::vector<scene_rdl2::rdl2::Vec3fVector> vec3fData;
        if (userData->hasVec3fData0() && useFirstFrame) {
            const scene_rdl2::rdl2::Vec3fVector& data0 = userData->getVec3fValues();
            vec3fData.push_back(data0);
        }
        if (userData->hasVec3fData1() && useSecondFrame) {
            const scene_rdl2::rdl2::Vec3fVector& data1 = userData->getVec3fValues1();
            vec3fData.push_back(data1);
        }
        if (!vec3fData.empty()) {
            const scene_rdl2::rdl2::String& key = userData->getVec3fKey();
            addCurvesPrimitiveAttribute(primitiveAttributeTable, key, partList, partName, std::move(vec3fData));
        }

        std::vector<scene_rdl2::rdl2::Mat4fVector> mat4fData;
        if (userData->hasMat4fData0() && useFirstFrame) {
            const scene_rdl2::rdl2::Mat4fVector& data0 = userData->getMat4fValues();
            mat4fData.push_back(data0);
        }
        if (userData->hasMat4fData1() && useSecondFrame) {
            const scene_rdl2::rdl2::Mat4fVector& data1 = userData->getMat4fValues1();
            mat4fData.push_back(data1);
        }
        if (!mat4fData.empty()) {
            const scene_rdl2::rdl2::String& key = userData->getMat4fKey();
            addCurvesPrimitiveAttribute(primitiveAttributeTable, key, partList, partName, std::move(mat4fData));
        }
    }
}



} // namespace geometry 
} // namespace moonshine 

