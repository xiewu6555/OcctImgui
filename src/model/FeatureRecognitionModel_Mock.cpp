/**
 * @file FeatureRecognitionModel_Mock.cpp
 * @brief Mock implementation for testing without IFR library
 */

#ifndef OCCTIMGUI_ENABLE_IFR

#include "FeatureRecognitionModel.h"
#include <sstream>
#include <nlohmann/json.hpp>
#include <BRepTools.hxx>
#include <TopExp_Explorer.hxx>

// 模拟的识别函数，用于测试界面功能
bool FeatureRecognitionModel::recognizeShape(const TopoDS_Shape& shape, const std::string& jsonParams)
{
    myOriginalShape = shape;
    buildFaceMap();

    // 创建符合 IFR 解析结构的模拟 JSON 结果
    nlohmann::json result;
    auto& parts = result["parts"];
    parts = nlohmann::json::array();

    nlohmann::json part;
    auto& featureRec = part["featureRecognition"];
    auto& groups = featureRec["featureGroups"];
    groups = nlohmann::json::array();

    // Group 1: Through Holes (direct features)
    nlohmann::json holeGroup;
    holeGroup["name"] = "Through Hole(s)";
    holeGroup["color"] = "(240, 135, 132)";
    holeGroup["totalGroupFeatureCount"] = 2;
    holeGroup["featureCount"] = 2;
    holeGroup["features"] = nlohmann::json::array({
        nlohmann::json{
            {"shapeIDCount", 1},
            {"shapeIDs", nlohmann::json::array({nlohmann::json{{"id", "1"}}})}},
        nlohmann::json{
            {"shapeIDCount", 1},
            {"shapeIDs", nlohmann::json::array({nlohmann::json{{"id", "3"}}})}}
    });
    groups.push_back(holeGroup);

    // Group 2: Closed Pockets (with sub-groups)
    nlohmann::json pocketGroup;
    pocketGroup["name"] = "Closed Pocket(s)";
    pocketGroup["color"] = "(150, 200, 150)";
    pocketGroup["totalGroupFeatureCount"] = 1;
    pocketGroup["subGroupCount"] = 1;

    nlohmann::json pocketSubGroup;
    pocketSubGroup["parametersCount"] = 2;
    pocketSubGroup["parameters"] = nlohmann::json::array({
        nlohmann::json{{"name", "Depth"}, {"units", "mm"}, {"value", "5.0"}},
        nlohmann::json{{"name", "Width"}, {"units", "mm"}, {"value", "10.0"}}
    });
    pocketSubGroup["featureCount"] = 1;
    pocketSubGroup["features"] = nlohmann::json::array({
        nlohmann::json{
            {"shapeIDCount", 2},
            {"shapeIDs",
             nlohmann::json::array({nlohmann::json{{"id", "5"}}, nlohmann::json{{"id", "6"}}})}}
    });

    pocketGroup["subGroups"] = nlohmann::json::array({pocketSubGroup});
    groups.push_back(pocketGroup);

    // Group 3: Chamfers (direct features)
    nlohmann::json chamferGroup;
    chamferGroup["name"] = "Chamfer(s)";
    chamferGroup["color"] = "(200, 200, 100)";
    chamferGroup["totalGroupFeatureCount"] = 1;
    chamferGroup["featureCount"] = 1;
    chamferGroup["features"] = nlohmann::json::array({
        nlohmann::json{
            {"shapeIDCount", 1},
            {"shapeIDs", nlohmann::json::array({nlohmann::json{{"id", "8"}}})}}
    });
    groups.push_back(chamferGroup);

    parts.push_back(part);

    myJsonResult = result.dump();
    return loadResultFromJson(myJsonResult);
}

#endif // OCCTIMGUI_ENABLE_IFR
