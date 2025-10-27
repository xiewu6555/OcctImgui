/**
 * @file FeatureRecognitionModel.h
 * @brief Model for managing CNC feature recognition results
 *
 * This model stores and manages feature recognition results from the IFR library.
 * It parses JSON results and provides structured access to recognized features.
 */
#pragma once

#include "IModel.h"
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <Quantity_Color.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopTools_DataMapOfShapeInteger.hxx>

/**
 * @class FeatureRecognitionModel
 * @brief Model for CNC feature recognition results
 *
 * This class manages feature recognition data from IFR library, including:
 * - Original CAD shape
 * - JSON recognition results
 * - Parsed feature groups and their geometry
 * - Face ID mapping for 3D visualization
 */
class FeatureRecognitionModel: public IModel
{
public:
    /**
     * @brief Structure representing a shape ID reference
     */
    struct ShapeID
    {
        std::string id;
    };

    /**
     * @brief Structure representing a feature parameter
     */
    struct Parameter
    {
        std::string name;   // "Radius", "Depth", etc.
        std::string units;  // "mm", "", etc.
        std::string value;  // "3.500000", "(0.00, 0.00, -1.00)", etc.
    };

    /**
     * @brief Structure representing a single feature instance
     */
    struct Feature
    {
        int shapeIDCount;
        std::vector<ShapeID> shapeIDs;
    };

    struct FeatureLocation
    {
        int groupIndex = -1;
        int subGroupIndex = -1; // -1 when group has direct features
        int featureIndex = -1;
    };

    /**
     * @brief Structure representing a feature sub-group (features with same parameters)
     */
    struct SubGroup
    {
        int parametersCount;
        std::vector<Parameter> parameters;
        int featureCount;
        std::vector<Feature> features;
    };

    /**
     * @brief Structure representing a feature group (e.g., "Through Hole(s)")
     */
    struct FeatureGroup
    {
        std::string name;                  // "Through Hole(s)", "Closed Pocket(s)", etc.
        std::string colorStr;              // "(240, 135, 132)" RGB string
        Quantity_Color color;              // Parsed OCC color
        int totalGroupFeatureCount;        // Total features in this group
        bool visible = true;               // Visibility toggle

        // Either has subGroups or direct features (mutually exclusive)
        std::optional<int> subGroupCount;
        std::optional<std::vector<SubGroup>> subGroups;

        std::optional<int> featureCount;
        std::optional<std::vector<Feature>> features;
    };

    /**
     * @brief Default constructor
     */
    FeatureRecognitionModel();

    /**
     * @brief Virtual destructor
     */
    ~FeatureRecognitionModel() override = default;

    // IModel interface implementation
    std::vector<std::string> getAllEntityIds() const override;
    void removeEntity(const std::string& id) override;

    /**
     * @brief Performs feature recognition on a CAD shape
     * @param shape The TopoDS_Shape to recognize
     * @param jsonParams Optional JSON parameters for recognition
     * @return true if recognition succeeded, false otherwise
     */
    bool recognizeShape(const TopoDS_Shape& shape, const std::string& jsonParams = "");

    /**
     * @brief Loads recognition results from JSON string
     * @param jsonString JSON result from CNC_FeatureRecognizer::getResultsAsJson()
     * @return true if parsing succeeded, false otherwise
     */
    bool loadResultFromJson(const std::string& jsonString);

    /**
     * @brief Gets the original shape that was recognized
     * @return The TopoDS_Shape
     */
    const TopoDS_Shape& getOriginalShape() const
    {
        return myOriginalShape;
    }

    /**
     * @brief Gets all feature groups
     * @return Vector of feature groups
     */
    const std::vector<FeatureGroup>& getFeatureGroups() const
    {
        return myFeatureGroups;
    }

    /**
     * @brief Gets face IDs for a specific feature
     * @param groupIdx Index of the feature group
     * @param subGroupIdx Index of the sub-group (if applicable, -1 for direct features)
     * @param featureIdx Index of the feature within the (sub-)group
     * @return Vector of face ID strings
     */
    std::vector<std::string> getFaceIDsForFeature(int groupIdx,
                                                   int subGroupIdx,
                                                   int featureIdx) const;

    /**
     * @brief Gets a face by its ID (1-based index from TopExp_Explorer)
     * @param faceID The face ID string
     * @return The TopoDS_Face, or null face if not found
     */
    TopoDS_Face getFaceByID(const std::string& faceID) const;

    /**
     * @brief 根据面实体查找对应的面 ID
     * @param face 目标面
     * @return 面 ID 字符串，若未找到返回空字符串
     */
    std::string getFaceId(const TopoDS_Face& face) const;

    std::vector<FeatureLocation> findFeatureLocationsForFace(const std::string& faceId) const;

    std::vector<FeatureLocation> findFeatureLocationsForFace(const TopoDS_Face& face) const;

    /**
     * @brief Builds the face ID map from the original shape
     */
    void buildFaceMap();

    /**
     * @brief Gets the last error message
     * @return Error description, or empty string if no error
     */
    std::string getLastError() const
    {
        return myLastError;
    }

    /**
     * @brief Checks if recognition results are available
     * @return true if results exist, false otherwise
     */
    bool hasResults() const
    {
        return !myFeatureGroups.empty();
    }

    /**
     * @brief Clears all recognition data
     */
    void clear();

private:
    TopoDS_Shape myOriginalShape;                  // The shape that was recognized
    std::string myJsonResult;                      // Raw JSON result
    std::vector<FeatureGroup> myFeatureGroups;     // Parsed feature groups
    std::map<std::string, TopoDS_Face> myFaceMap;  // Map from face ID to TopoDS_Face
    TopTools_DataMapOfShapeInteger myFaceReverseMap; // Map from TopoDS_Face to face index
    std::unordered_map<std::string, std::vector<FeatureLocation>> myFaceToFeatureMap;
    std::string myLastError;                       // Last error message

    /**
     * @brief Serializes a TopoDS_Shape to string using BRepTools
     * @param shape The shape to serialize
     * @return Serialized string
     */
    std::string serializeShape(const TopoDS_Shape& shape);

    /**
     * @brief Parses RGB color string like "(240, 135, 132)" to Quantity_Color
     * @param colorStr The color string
     * @return Parsed color
     */
    Quantity_Color parseColorString(const std::string& colorStr);

    /**
     * @brief Parses JSON result and populates myFeatureGroups
     * @param jsonString The JSON string to parse
     * @return true if successful, false otherwise
     */
    bool parseJsonResult(const std::string& jsonString);
};
