#include "SelectionManager.h"
#include "utils/Logger.h"

#include <AIS_Shape.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>

#include <algorithm>
#include <set>
#include <string>
#include <unordered_set>

using namespace MVVM;

// 使用宏声明 SelectionManager 类的 logger
DECLARE_LOGGER(SelectionManager)

SelectionManager& SelectionManager::getInstance()
{
    static SelectionManager instance;
    return instance;
}

SelectionManager::SelectionManager()
    : myMessageBus(MessageBus::getInstance())
{
    // Initialize selection info
    mySelectionInfo.selectionMode = 0;
    mySelectionInfo.selectionType = SelectionInfo::SelectionType::New;
    getSelectionManagerLogger()->info("SelectionManager initialized with New selection type");
}

void SelectionManager::addToSelection(const Handle(AIS_InteractiveObject) & object,
                                      const std::string& objectId)
{
    auto logger = getSelectionManagerLogger();
    logger->info("Adding object {} to selection (type: {})",
                 objectId,
                 mySelectionInfo.selectionType == SelectionInfo::SelectionType::New ? "New"
                     : mySelectionInfo.selectionType == SelectionInfo::SelectionType::Add
                     ? "Add"
                     : "Remove");

    auto findByObject = [&](const Handle(AIS_InteractiveObject) & candidate) {
        return std::find_if(mySelectionInfo.selectedObjects.begin(),
                            mySelectionInfo.selectedObjects.end(),
                            [&](const SelectionInfo::SelectedObject& entry) {
                                return entry.object == candidate;
                            });
    };

    if (mySelectionInfo.selectionType == SelectionInfo::SelectionType::New) {
        mySelectionInfo.selectedObjects.clear();
        mySelectionInfo.subFeatures.clear();
        logger->debug("Cleared previous selection (New type)");
    }

    if (mySelectionInfo.selectionType != SelectionInfo::SelectionType::Remove) {
        auto it = findByObject(object);
        if (it == mySelectionInfo.selectedObjects.end()) {
            mySelectionInfo.selectedObjects.emplace_back(objectId, object);
            logger->debug("Added object {} to selection. Current selection size: {}",
                          objectId,
                          mySelectionInfo.selectedObjects.size());
        }
        else {
            it->id = objectId;
            logger->debug("Object {} already in selection, updated identifier", objectId);
        }
    }
    else {
        auto it = findByObject(object);
        if (it != mySelectionInfo.selectedObjects.end()) {
            mySelectionInfo.subFeatures.erase(it->id);
            mySelectionInfo.selectedObjects.erase(it);
            logger->debug("Removed object {} from selection. Current selection size: {}",
                          objectId,
                          mySelectionInfo.selectedObjects.size());
        }
        else {
            logger->debug("Object {} not found in selection for removal", objectId);
        }
    }

    notifySelectionChanged();
}

void SelectionManager::addToSelection(
    const Handle(AIS_InteractiveObject) & object,
    const std::string& objectId,
    const std::vector<SelectionInfo::SubFeatureIdentifier>& subFeatures)
{
    auto logger = getSelectionManagerLogger();
    logger->info("Adding object {} with {} subfeatures to selection", objectId, subFeatures.size());

    // First add the object to selection
    addToSelection(object, objectId);

    // Add subfeature information
    if (mySelectionInfo.selectionType != SelectionInfo::SelectionType::Remove) {
        mySelectionInfo.subFeatures[objectId] = subFeatures;
        logger->debug("Added {} subfeatures for object {}", subFeatures.size(), objectId);

        // Log subfeature details
        for (const auto& subFeature : subFeatures) {
            logger->debug("  - Type: {}, Index: {}",
                          subFeature.type == SelectionInfo::SubFeatureType::Face       ? "Face"
                              : subFeature.type == SelectionInfo::SubFeatureType::Edge ? "Edge"
                                                                                       : "Vertex",
                          subFeature.index);
        }
    }

    // Notify selection changed
    notifySelectionChanged();
}

void SelectionManager::removeFromSelection(const Handle(AIS_InteractiveObject) & object,
                                           const std::string& objectId)
{
    auto logger = getSelectionManagerLogger();
    logger->info("Removing object {} from selection", objectId);

    auto it = std::find_if(mySelectionInfo.selectedObjects.begin(),
                           mySelectionInfo.selectedObjects.end(),
                           [&](const SelectionInfo::SelectedObject& entry) {
                               return entry.object == object;
                           });

    if (it == mySelectionInfo.selectedObjects.end()) {
        logger->debug("Object {} not found in selection for removal", objectId);
        return;
    }

    mySelectionInfo.subFeatures.erase(it->id);
    mySelectionInfo.selectedObjects.erase(it);
    logger->debug("Removed object {} and its subfeatures. Current selection size: {}",
                  objectId,
                  mySelectionInfo.selectedObjects.size());

    notifySelectionChanged();
}

void SelectionManager::removeFromSelection(const std::string& objectId)
{
    auto logger = getSelectionManagerLogger();
    logger->info("Removing object {} from selection by ID", objectId);

    auto it = std::find_if(mySelectionInfo.selectedObjects.begin(),
                           mySelectionInfo.selectedObjects.end(),
                           [&](const SelectionInfo::SelectedObject& entry) {
                               return entry.id == objectId;
                           });

    if (it == mySelectionInfo.selectedObjects.end()) {
        logger->debug("Object {} not found in selection for removal", objectId);
        return;
    }

    mySelectionInfo.subFeatures.erase(objectId);
    mySelectionInfo.selectedObjects.erase(it);
    logger->debug("Removed object {} from selection. Current selection size: {}",
                  objectId,
                  mySelectionInfo.selectedObjects.size());

    notifySelectionChanged();
}

void SelectionManager::clearSelection()
{
    auto logger = getSelectionManagerLogger();
    logger->info("Clearing all selections");

    mySelectionInfo.selectedObjects.clear();
    mySelectionInfo.subFeatures.clear();
    logger->debug("Cleared {} objects and {} subfeature entries",
                  mySelectionInfo.selectedObjects.size(),
                  mySelectionInfo.subFeatures.size());

    // Notify selection changed
    notifySelectionChanged();
}

void SelectionManager::setSelectionMode(int mode)
{
    auto logger = getSelectionManagerLogger();
    logger->info("Setting selection mode to {}", mode);
    mySelectionInfo.selectionMode = mode;
}

void SelectionManager::setSelectionType(SelectionInfo::SelectionType type)
{
    auto logger = getSelectionManagerLogger();
    logger->info("Setting selection type to {}",
                 type == SelectionInfo::SelectionType::New       ? "New"
                     : type == SelectionInfo::SelectionType::Add ? "Add"
                                                                 : "Remove");
    mySelectionInfo.selectionType = type;
}

const SelectionInfo& SelectionManager::getCurrentSelection() const
{
    return mySelectionInfo;
}

bool SelectionManager::hasSelection() const
{
    return !mySelectionInfo.selectedObjects.empty();
}

TopoDS_Shape SelectionManager::getSelectedShape() const
{
    if (mySelectionInfo.selectedObjects.empty()) {
        return TopoDS_Shape();
    }

    Handle(AIS_Shape) aisShape =
        Handle(AIS_Shape)::DownCast(mySelectionInfo.selectedObjects.front().object);
    if (!aisShape.IsNull()) {
        return aisShape->Shape();
    }

    return TopoDS_Shape();
}

std::vector<TopoDS_Face> SelectionManager::getSelectedFaces() const
{
    std::vector<TopoDS_Face> faces;
    for (const auto& entry : mySelectionInfo.selectedObjects) {
        Handle(AIS_Shape) aisShape = Handle(AIS_Shape)::DownCast(entry.object);
        if (aisShape.IsNull()) {
            continue;
        }

        const TopoDS_Shape& parentShape = aisShape->Shape();
        if (parentShape.IsNull()) {
            continue;
        }

        TopTools_IndexedMapOfShape faceMap;
        TopExp::MapShapes(parentShape, TopAbs_FACE, faceMap);

        std::set<int> visitedIndices;
        auto subFeatureIt = mySelectionInfo.subFeatures.find(entry.id);
        if (subFeatureIt == mySelectionInfo.subFeatures.end()) {
            continue;
        }

        for (const auto& subFeature : subFeatureIt->second) {
            if (subFeature.type != SelectionInfo::SubFeatureType::Face) {
                continue;
            }

            if (subFeature.additionalData.has_value()) {
                try {
                    const auto& data =
                        std::any_cast<const SelectionInfo::FaceSelectionData&>(subFeature.additionalData);
                    if (!data.face.IsNull()) {
                        faces.push_back(data.face);
                        continue;
                    }
                }
                catch (const std::bad_any_cast&) {
                    try {
                        faces.push_back(std::any_cast<TopoDS_Face>(subFeature.additionalData));
                        continue;
                    }
                    catch (const std::bad_any_cast&) {
                        // Fall through to index-based lookup
                    }
                }
            }

            if (subFeature.index <= 0 || subFeature.index > faceMap.Extent()) {
                continue;
            }

            if (visitedIndices.insert(subFeature.index).second) {
                faces.push_back(TopoDS::Face(faceMap(subFeature.index)));
            }
        }
    }

    return faces;
}

std::vector<std::string> SelectionManager::getSelectedFaceIds() const
{
    std::vector<std::string> ids;
    std::unordered_set<std::string> seen;

    for (const auto& entry : mySelectionInfo.selectedObjects) {
        auto subFeatureIt = mySelectionInfo.subFeatures.find(entry.id);
        if (subFeatureIt == mySelectionInfo.subFeatures.end()) {
            continue;
        }

        for (const auto& subFeature : subFeatureIt->second) {
            if (subFeature.type != SelectionInfo::SubFeatureType::Face) {
                continue;
            }

            std::string faceId;

            if (subFeature.additionalData.has_value()) {
                try {
                    const auto& data =
                        std::any_cast<const SelectionInfo::FaceSelectionData&>(subFeature.additionalData);
                    faceId = data.id;
                }
                catch (const std::bad_any_cast&) {
                    try {
                        faceId = std::any_cast<std::string>(subFeature.additionalData);
                    }
                    catch (const std::bad_any_cast&) {
                        // ignore
                    }
                }
            }

            if (faceId.empty() && subFeature.index > 0) {
                faceId = std::to_string(subFeature.index);
            }

            if (!faceId.empty() && seen.insert(faceId).second) {
                ids.push_back(std::move(faceId));
            }
        }
    }

    return ids;
}

int SelectionManager::getSelectionMode() const
{
    return mySelectionInfo.selectionMode;
}

void SelectionManager::setSelection(
    const std::vector<SelectionInfo::SelectedObject>& objects,
    const std::map<std::string, std::vector<SelectionInfo::SubFeatureIdentifier>>& subFeatures,
    SelectionInfo::SelectionType type)
{
    mySelectionInfo.selectedObjects = objects;
    mySelectionInfo.subFeatures     = subFeatures;
    mySelectionInfo.selectionType   = type;
    notifySelectionChanged();
}

void SelectionManager::notifySelectionChanged()
{
    auto logger = getSelectionManagerLogger();
    logger->debug("Notifying selection changed: {} objects, {} subfeature entries",
                  mySelectionInfo.selectedObjects.size(),
                  mySelectionInfo.subFeatures.size());

    // Create a message with the selection info
    MessageBus::Message message;
    message.type = MessageBus::MessageType::SelectionChanged;
    message.data = mySelectionInfo;

    // Publish the message
    myMessageBus.publish(message);
}
