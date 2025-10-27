/**
 * @file FeatureRecognitionModel.cpp
 * @brief Implementation of FeatureRecognitionModel
 */

#include "FeatureRecognitionModel.h"
#include "utils/Logger.h"

#include <nlohmann/json.hpp>
#include <sstream>
#include <regex>
#include <limits>
#include <unordered_map>
#include <unordered_set>

#include <BRepTools.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs.hxx>
#include <TopoDS.hxx>

#ifdef OCCTIMGUI_ENABLE_IFR
#include <asiAlgo_ShapeSerializer.h>
#include <featureRecognizer/CNC_FeatureRecognizer.h>
#endif

using json = nlohmann::json;

namespace
{
int parseIntFlexible(const json& value, int defaultValue)
{
    if (value.is_number_integer())
    {
        return value.get<int>();
    }
    if (value.is_number_unsigned())
    {
        const auto unsignedVal = value.get<unsigned long long>();
        if (unsignedVal > static_cast<unsigned long long>(std::numeric_limits<int>::max()))
        {
            return defaultValue;
        }
        return static_cast<int>(unsignedVal);
    }
    if (value.is_number_float())
    {
        return static_cast<int>(value.get<double>());
    }
    if (value.is_boolean())
    {
        return value.get<bool>() ? 1 : 0;
    }
    if (value.is_string())
    {
        const auto& text = value.get_ref<const std::string&>();
        try
        {
            size_t consumed = 0;
            int parsed       = std::stoi(text, &consumed);
            if (consumed == text.size())
            {
                return parsed;
            }
        }
        catch (...)
        {
        }
    }
    return defaultValue;
}

int getIntValue(const json& object, const char* key, int defaultValue = 0)
{
    const auto it = object.find(key);
    if (it == object.end() || it->is_null())
    {
        return defaultValue;
    }
    return parseIntFlexible(*it, defaultValue);
}

std::string jsonToString(const json& value)
{
    if (value.is_string())
    {
        return value.get<std::string>();
    }
    if (value.is_number_integer())
    {
        return std::to_string(value.get<long long>());
    }
    if (value.is_number_unsigned())
    {
        return std::to_string(value.get<unsigned long long>());
    }
    if (value.is_number_float())
    {
        std::ostringstream oss;
        oss << value.get<double>();
        return oss.str();
    }
    if (value.is_boolean())
    {
        return value.get<bool>() ? "true" : "false";
    }
    return value.dump();
}

std::string getStringValue(const json& object,
                           const char* key,
                           const std::string& defaultValue = {})
{
    const auto it = object.find(key);
    if (it == object.end() || it->is_null())
    {
        return defaultValue;
    }
    return jsonToString(*it);
}
} // namespace

//=============================================================================
// Constructor
//=============================================================================
FeatureRecognitionModel::FeatureRecognitionModel()
{
    Utils::Logger::getLogger("Model")->debug("FeatureRecognitionModel created");
}

//=============================================================================
// IModel interface implementation
//=============================================================================
std::vector<std::string> FeatureRecognitionModel::getAllEntityIds() const
{
    // Return a single entity ID representing the recognition result
    if (hasResults())
    {
        return {"feature_recognition_result"};
    }
    return {};
}

void FeatureRecognitionModel::removeEntity(const std::string& id)
{
    if (id == "feature_recognition_result")
    {
        clear();
        notifyChange("feature_recognition_result");
    }
}

//=============================================================================
// Feature Recognition
//=============================================================================
#ifdef OCCTIMGUI_ENABLE_IFR
bool FeatureRecognitionModel::recognizeShape(const TopoDS_Shape& shape,
                                              const std::string& jsonParams)
{
    auto logger = Utils::Logger::getLogger("Model");
    logger->info("Starting feature recognition");

    try
    {
        myOriginalShape = shape;
        buildFaceMap();

        // Serialize shape to string
        std::string shapeStr = serializeShape(shape);
        if (shapeStr.empty())
        {
            myLastError = "Failed to serialize shape";
            logger->error(myLastError);
            return false;
        }

        CNC_FeatureRecognizer recognizer;
        if (!recognizer.loadModelFromString(shapeStr, false))
        {
            myLastError = "Failed to load model: " + recognizer.getLastError();
            logger->error(myLastError);
            return false;
        }

        // Set parameters if provided
        if (!jsonParams.empty())
        {
            if (!recognizer.setParameters(jsonParams))
            {
                myLastError = "Failed to set parameters: " + recognizer.getLastError();
                logger->error(myLastError);
                return false;
            }
        }

        // Perform recognition
        if (!recognizer.recognize())
        {
            myLastError = "Recognition failed: " + recognizer.getLastError();
            logger->error(myLastError);
            return false;
        }

        // Get results as JSON
        myJsonResult = recognizer.getResultsAsJson();
        if (myJsonResult.empty())
        {
            myLastError = "Empty recognition result";
            logger->error(myLastError);
            return false;
        }

        // Parse JSON result
        if (!parseJsonResult(myJsonResult))
        {
            logger->error("Failed to parse JSON result");
            return false;
        }

        logger->info("Feature recognition completed successfully, found {} feature groups",
                     myFeatureGroups.size());
        notifyChange("feature_recognition_result");
        return true;
    }
    catch (const std::exception& e)
    {
        myLastError = std::string("Exception during recognition: ") + e.what();
        logger->error(myLastError);
        return false;
    }
}
#endif // OCCTIMGUI_ENABLE_IFR

bool FeatureRecognitionModel::loadResultFromJson(const std::string& jsonString)
{
    myJsonResult = jsonString;
    return parseJsonResult(jsonString);
}

//=============================================================================
// Face Map Management
//=============================================================================
void FeatureRecognitionModel::buildFaceMap()
{
    auto logger = Utils::Logger::getLogger("Model");
    myFaceMap.clear();
    myFaceReverseMap.Clear();

    if (myOriginalShape.IsNull())
    {
        logger->warn("Cannot build face map: original shape is null");
        return;
    }

    int faceIndex = 1;  // IFR uses 1-based indexing
    for (TopExp_Explorer exp(myOriginalShape, TopAbs_FACE); exp.More(); exp.Next())
    {
        TopoDS_Face face = TopoDS::Face(exp.Current());
        myFaceMap[std::to_string(faceIndex)] = face;

        TopoDS_Face normalizedFace = face;
        normalizedFace.Orientation(TopAbs_FORWARD);
        myFaceReverseMap.Bind(normalizedFace, faceIndex);
        faceIndex++;
    }

    logger->debug("Built face map with {} faces", myFaceMap.size());
}

TopoDS_Face FeatureRecognitionModel::getFaceByID(const std::string& faceID) const
{
    auto it = myFaceMap.find(faceID);
    if (it != myFaceMap.end())
    {
        return it->second;
    }
    return TopoDS_Face();  // Return null face
}

std::string FeatureRecognitionModel::getFaceId(const TopoDS_Face& face) const
{
    if (face.IsNull())
    {
        return std::string();
    }

    TopoDS_Face normalizedFace = face;
    normalizedFace.Orientation(TopAbs_FORWARD);

    if (myFaceReverseMap.IsBound(normalizedFace))
    {
        return std::to_string(myFaceReverseMap.Find(normalizedFace));
    }

    for (const auto& entry : myFaceMap)
    {
        if (face.IsSame(entry.second))
        {
            return entry.first;
        }
    }

    return std::string();
}

std::vector<FeatureRecognitionModel::FeatureLocation>
FeatureRecognitionModel::findFeatureLocationsForFace(const std::string& faceId) const
{
    auto it = myFaceToFeatureMap.find(faceId);
    if (it != myFaceToFeatureMap.end())
    {
        return it->second;
    }
    return {};
}

std::vector<FeatureRecognitionModel::FeatureLocation>
FeatureRecognitionModel::findFeatureLocationsForFace(const TopoDS_Face& face) const
{
    return findFeatureLocationsForFace(getFaceId(face));
}

bool FeatureRecognitionModel::toggleGroupVisibility(int groupIdx)
{
    if (groupIdx < 0 || groupIdx >= static_cast<int>(myFeatureGroups.size()))
    {
        return true;
    }

    auto& group = myFeatureGroups[groupIdx];
    group.visible = !group.visible;
    notifyChange("feature_recognition_result");
    return group.visible;
}

bool FeatureRecognitionModel::isGroupVisible(int groupIdx) const
{
    if (groupIdx < 0 || groupIdx >= static_cast<int>(myFeatureGroups.size()))
    {
        return true;
    }
    return myFeatureGroups[groupIdx].visible;
}

std::vector<std::string> FeatureRecognitionModel::getFaceIDsForFeature(int groupIdx,
                                                                        int subGroupIdx,
                                                                        int featureIdx) const
{
    std::vector<std::string>        faceIDs;
    std::unordered_set<std::string> uniqueIds;

    if (groupIdx < 0 || groupIdx >= static_cast<int>(myFeatureGroups.size()))
    {
        return faceIDs;
    }

    const auto& group = myFeatureGroups[groupIdx];

    auto appendFeatureFaces = [&uniqueIds](const Feature& feature) {
        for (const auto& shapeID : feature.shapeIDs)
        {
            if (!shapeID.id.empty())
            {
                uniqueIds.insert(shapeID.id);
            }
        }
    };

    auto appendSubGroupFaces = [&](const SubGroup& subGroup) {
        for (const auto& feature : subGroup.features)
        {
            appendFeatureFaces(feature);
        }
    };

    // Group with sub-groups
    if (group.subGroups.has_value())
    {
        const auto& subGroups = group.subGroups.value();

        // Specific sub-group requested
        if (subGroupIdx >= 0 && subGroupIdx < static_cast<int>(subGroups.size()))
        {
            const auto& subGroup = subGroups[subGroupIdx];

            if (featureIdx >= 0 && featureIdx < static_cast<int>(subGroup.features.size()))
            {
                appendFeatureFaces(subGroup.features[featureIdx]);
            }
            else if (featureIdx < 0)
            {
                appendSubGroupFaces(subGroup);
            }
        }
        // Whole group requested
        else if (subGroupIdx < 0)
        {
            for (const auto& subGroup : subGroups)
            {
                appendSubGroupFaces(subGroup);
            }
        }
    }
    // Direct features without sub-groups
    else if (group.features.has_value())
    {
        const auto& features = group.features.value();

        if (featureIdx >= 0 && featureIdx < static_cast<int>(features.size()))
        {
            appendFeatureFaces(features[featureIdx]);
        }
        else if (featureIdx < 0)
        {
            for (const auto& feature : features)
            {
                appendFeatureFaces(feature);
            }
        }
    }

    faceIDs.assign(uniqueIds.begin(), uniqueIds.end());
    return faceIDs;
}

//=============================================================================
// Serialization
//=============================================================================
std::string FeatureRecognitionModel::serializeShape(const TopoDS_Shape& shape)
{
    auto logger = Utils::Logger::getLogger("Model");

    if (shape.IsNull())
    {
        logger->warn("Cannot serialize null shape");
        return "";
    }

    try
    {
#ifdef OCCTIMGUI_ENABLE_IFR
        std::string serializedShape;
        if (!asiAlgo_ShapeSerializer::Serialize(shape, serializedShape, false))
        {
            logger->error("asiAlgo_ShapeSerializer failed to serialize shape");
            return "";
        }
        return serializedShape;
#else
        std::ostringstream oss;
        BRepTools::Write(shape, oss);
        return oss.str();
#endif
    }
    catch (const std::exception& e)
    {
        logger->error("Exception during shape serialization: {}", e.what());
        return "";
    }
}

//=============================================================================
// JSON Parsing
//=============================================================================
Quantity_Color FeatureRecognitionModel::parseColorString(const std::string& colorStr)
{
    // Parse color string like "(240, 135, 132)"
    std::regex rgbRegex(R"(\((\d+),\s*(\d+),\s*(\d+)\))");
    std::smatch match;

    if (std::regex_match(colorStr, match, rgbRegex) && match.size() == 4)
    {
        int r = std::stoi(match[1].str());
        int g = std::stoi(match[2].str());
        int b = std::stoi(match[3].str());

        // Convert 0-255 to 0.0-1.0
        return Quantity_Color(r / 255.0, g / 255.0, b / 255.0, Quantity_TOC_RGB);
    }

    // Default to gray if parsing fails
    return Quantity_Color(0.5, 0.5, 0.5, Quantity_TOC_RGB);
}

bool FeatureRecognitionModel::parseJsonResult(const std::string& jsonString)
{
    auto logger = Utils::Logger::getLogger("Model");

    try
    {
        json j = json::parse(jsonString);

        // Clear existing data
        myFeatureGroups.clear();
        myFaceToFeatureMap.clear();

        // Navigate to featureGroups array
        if (!j.contains("parts") || !j["parts"].is_array() || j["parts"].empty())
        {
            myLastError = "JSON missing 'parts' array";
            logger->error(myLastError);
            return false;
        }

        const auto& part = j["parts"][0];
        if (!part.contains("featureRecognition"))
        {
            myLastError = "JSON missing 'featureRecognition' object";
            logger->error(myLastError);
            return false;
        }

        const auto& featureRec = part["featureRecognition"];
        if (!featureRec.contains("featureGroups") || !featureRec["featureGroups"].is_array())
        {
            myLastError = "JSON missing 'featureGroups' array";
            logger->error(myLastError);
            return false;
        }

        // Parse each feature group
        for (const auto& jGroup : featureRec["featureGroups"])
        {
            FeatureGroup group;

            // Parse basic properties
            group.name                  = getStringValue(jGroup, "name", "");
            group.colorStr              = getStringValue(jGroup, "color", "(128, 128, 128)");
            group.color                 = parseColorString(group.colorStr);
            group.totalGroupFeatureCount =
              getIntValue(jGroup, "totalGroupFeatureCount", 0);

            // Check for subGroups
            if (jGroup.contains("subGroups") && jGroup["subGroups"].is_array())
            {
                group.subGroupCount = getIntValue(jGroup, "subGroupCount", 0);
                std::vector<SubGroup> subGroups;

                for (const auto& jSubGroup : jGroup["subGroups"])
                {
                    SubGroup subGroup;
                    subGroup.parametersCount = getIntValue(jSubGroup, "parametersCount", 0);
                    subGroup.featureCount    = getIntValue(jSubGroup, "featureCount", 0);

                    // Parse parameters
                    if (jSubGroup.contains("parameters") && jSubGroup["parameters"].is_array())
                    {
                        for (const auto& jParam : jSubGroup["parameters"])
                        {
                            Parameter param;
                            param.name  = getStringValue(jParam, "name", "");
                            param.units = getStringValue(jParam, "units", "");
                            param.value = getStringValue(jParam, "value", "");
                            subGroup.parameters.push_back(param);
                        }
                    }

                    // Parse features
                    if (jSubGroup.contains("features") && jSubGroup["features"].is_array())
                    {
                        for (const auto& jFeature : jSubGroup["features"])
                        {
                            Feature feature;
                            feature.shapeIDCount = getIntValue(jFeature, "shapeIDCount", 0);

                            if (jFeature.contains("shapeIDs") && jFeature["shapeIDs"].is_array())
                            {
                                for (const auto& jShapeID : jFeature["shapeIDs"])
                                {
                                    ShapeID shapeID;
                                    shapeID.id = getStringValue(jShapeID, "id", "");
                                    feature.shapeIDs.push_back(shapeID);
                                }
                            }

                            subGroup.features.push_back(feature);
                        }
                    }

                    subGroups.push_back(subGroup);
                }

                group.subGroups = subGroups;
            }
            // Otherwise check for direct features
            else if (jGroup.contains("features") && jGroup["features"].is_array())
            {
                group.featureCount = getIntValue(jGroup, "featureCount", 0);
                std::vector<Feature> features;

                for (const auto& jFeature : jGroup["features"])
                {
                    Feature feature;
                    feature.shapeIDCount = getIntValue(jFeature, "shapeIDCount", 0);

                    if (jFeature.contains("shapeIDs") && jFeature["shapeIDs"].is_array())
                    {
                        for (const auto& jShapeID : jFeature["shapeIDs"])
                        {
                            ShapeID shapeID;
                            shapeID.id = getStringValue(jShapeID, "id", "");
                            feature.shapeIDs.push_back(shapeID);
                        }
                    }

                    features.push_back(feature);
                }

                group.features = features;
            }

            myFeatureGroups.push_back(group);
        }

        // Build face-to-feature lookup table
        for (size_t groupIdx = 0; groupIdx < myFeatureGroups.size(); ++groupIdx)
        {
            const auto& group = myFeatureGroups[groupIdx];

            if (group.subGroups.has_value())
            {
                const auto& subGroups = group.subGroups.value();
                for (size_t subGroupIdx = 0; subGroupIdx < subGroups.size(); ++subGroupIdx)
                {
                    const auto& subGroup = subGroups[subGroupIdx];
                    for (size_t featureIdx = 0; featureIdx < subGroup.features.size(); ++featureIdx)
                    {
                        const auto& feature = subGroup.features[featureIdx];
                        FeatureLocation location{static_cast<int>(groupIdx),
                                                 static_cast<int>(subGroupIdx),
                                                 static_cast<int>(featureIdx)};
                        for (const auto& shapeID : feature.shapeIDs)
                        {
                            if (!shapeID.id.empty())
                            {
                                myFaceToFeatureMap[shapeID.id].push_back(location);
                            }
                        }
                    }
                }
            }
            else if (group.features.has_value())
            {
                const auto& features = group.features.value();
                for (size_t featureIdx = 0; featureIdx < features.size(); ++featureIdx)
                {
                    const auto& feature = features[featureIdx];
                    FeatureLocation location{static_cast<int>(groupIdx),
                                             -1,
                                             static_cast<int>(featureIdx)};
                    for (const auto& shapeID : feature.shapeIDs)
                    {
                        if (!shapeID.id.empty())
                        {
                            myFaceToFeatureMap[shapeID.id].push_back(location);
                        }
                    }
                }
            }
        }

        logger->info("Parsed {} feature groups from JSON", myFeatureGroups.size());
        return true;
    }
    catch (const json::exception& e)
    {
        myLastError = std::string("JSON parsing error: ") + e.what();
        logger->error(myLastError);
        return false;
    }
    catch (const std::exception& e)
    {
        myLastError = std::string("Exception during JSON parsing: ") + e.what();
        logger->error(myLastError);
        return false;
    }
}

//=============================================================================
// Clear
//=============================================================================
void FeatureRecognitionModel::clear()
{
    myOriginalShape.Nullify();
    myJsonResult.clear();
    myFeatureGroups.clear();
    myFaceMap.clear();
    myFaceReverseMap.Clear();
    myFaceToFeatureMap.clear();
    myLastError.clear();
}
