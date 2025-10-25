/**
 * @file FeatureRecognitionViewModel.cpp
 * @brief Implementation of FeatureRecognitionViewModel
 */

#include "FeatureRecognitionViewModel.h"
#include "mvvm/MessageBus.h"
#include "utils/Logger.h"

//=============================================================================
// Constructor
//=============================================================================
FeatureRecognitionViewModel::FeatureRecognitionViewModel(
    std::shared_ptr<FeatureRecognitionModel> model)
    : myModel(model)
{
    auto logger = Utils::Logger::getLogger("ViewModel");
    logger->debug("FeatureRecognitionViewModel created");

    subscribeToModelEvents();
    updateProperties();
}

//=============================================================================
// Commands
//=============================================================================
void FeatureRecognitionViewModel::executeRecognition(const TopoDS_Shape& shape,
                                                       const std::string& jsonParams)
{
    auto logger = Utils::Logger::getLogger("ViewModel");
    logger->info("Executing feature recognition");

    isRecognizing.set(true);
    statusMessage.set("Recognizing features...");
    onRecognitionStarted.emit();

    // Execute recognition in the model
    bool success = myModel->recognizeShape(shape, jsonParams);

    isRecognizing.set(false);

    if (success)
    {
        statusMessage.set("Recognition completed successfully");
        updateProperties();
        onRecognitionCompleted.emit();

        logger->info("Feature recognition completed, found {} groups",
                     myModel->getFeatureGroups().size());

        // Publish message to message bus
        MVVM::MessageBus::Message message;
        message.type = MVVM::MessageBus::MessageType::ModelChanged;
        message.data = myModel;
        MVVM::MessageBus::getInstance().publish(message);
    }
    else
    {
        std::string error = "Recognition failed: " + myModel->getLastError();
        statusMessage.set(error);
        onRecognitionFailed.emit(error);

        logger->error("Feature recognition failed: {}", myModel->getLastError());
    }
}

void FeatureRecognitionViewModel::loadResultsFromJson(const std::string& jsonString)
{
    auto logger = Utils::Logger::getLogger("ViewModel");
    logger->info("Loading feature recognition results from JSON");

    bool success = myModel->loadResultFromJson(jsonString);

    if (success)
    {
        statusMessage.set("Results loaded successfully");
        updateProperties();
        logger->info("Loaded {} feature groups", myModel->getFeatureGroups().size());

        // Publish message to message bus
        MVVM::MessageBus::Message message;
        message.type = MVVM::MessageBus::MessageType::ModelChanged;
        message.data = myModel;
        MVVM::MessageBus::getInstance().publish(message);
    }
    else
    {
        std::string error = "Failed to load results: " + myModel->getLastError();
        statusMessage.set(error);
        logger->error(error);
    }
}

void FeatureRecognitionViewModel::selectFeature(int groupIdx, int subGroupIdx, int featureIdx)
{
    auto logger = Utils::Logger::getLogger("ViewModel");
    logger->debug("Selecting feature: group={}, subGroup={}, feature={}",
                  groupIdx,
                  subGroupIdx,
                  featureIdx);

    selectedGroupIndex.set(groupIdx);
    selectedSubGroupIndex.set(subGroupIdx);
    selectedFeatureIndex.set(featureIdx);

    onFeatureSelected.emit(groupIdx, subGroupIdx, featureIdx);
}

void FeatureRecognitionViewModel::toggleFeatureGroupVisibility(int groupIdx)
{
    auto logger = Utils::Logger::getLogger("ViewModel");

    const auto& groups = myModel->getFeatureGroups();
    if (groupIdx < 0 || groupIdx >= static_cast<int>(groups.size()))
    {
        logger->warn("Invalid feature group index: {}", groupIdx);
        return;
    }

    // Note: We need to modify the model to support visibility
    // For now, just emit the signal
    bool newVisibility = !groups[groupIdx].visible;
    logger->debug("Toggling feature group {} visibility to {}", groupIdx, newVisibility);

    onFeatureVisibilityChanged.emit(groupIdx, newVisibility);
}

void FeatureRecognitionViewModel::clearSelection()
{
    selectedGroupIndex.set(-1);
    selectedSubGroupIndex.set(-1);
    selectedFeatureIndex.set(-1);

    onFeatureSelected.emit(-1, -1, -1);
}

void FeatureRecognitionViewModel::clearResults()
{
    auto logger = Utils::Logger::getLogger("ViewModel");
    logger->info("Clearing feature recognition results");

    myModel->clear();
    clearSelection();
    updateProperties();

    statusMessage.set("Results cleared");

    // Publish message to message bus
    MVVM::MessageBus::Message message;
    message.type = MVVM::MessageBus::MessageType::ModelChanged;
    message.data = myModel;
    MVVM::MessageBus::getInstance().publish(message);
}

//=============================================================================
// Query Methods
//=============================================================================
const std::vector<FeatureRecognitionModel::FeatureGroup>&
FeatureRecognitionViewModel::getFeatureGroups() const
{
    return myModel->getFeatureGroups();
}

std::vector<std::string> FeatureRecognitionViewModel::getSelectedFeatureFaceIDs() const
{
    return myModel->getFaceIDsForFeature(selectedGroupIndex.get(),
                                         selectedSubGroupIndex.get(),
                                         selectedFeatureIndex.get());
}

std::vector<std::string>
FeatureRecognitionViewModel::getFeatureFaceIDs(int groupIdx, int subGroupIdx, int featureIdx) const
{
    return myModel->getFaceIDsForFeature(groupIdx, subGroupIdx, featureIdx);
}

Quantity_Color FeatureRecognitionViewModel::getFeatureGroupColor(int groupIdx) const
{
    const auto& groups = myModel->getFeatureGroups();
    if (groupIdx >= 0 && groupIdx < static_cast<int>(groups.size()))
    {
        return groups[groupIdx].color;
    }

    // Default to gray
    return Quantity_Color(0.5, 0.5, 0.5, Quantity_TOC_RGB);
}

std::vector<FeatureRecognitionModel::FeatureLocation>
FeatureRecognitionViewModel::findFeatureLocationsForFace(const std::string& faceId) const
{
    if (!myModel)
    {
        return {};
    }
    return myModel->findFeatureLocationsForFace(faceId);
}

//=============================================================================
// Private Methods
//=============================================================================
void FeatureRecognitionViewModel::subscribeToModelEvents()
{
    // Subscribe to model change signals
    if (!myModel) {
        return;
    }

    myModel->addChangeListener([this](const std::string&) {
        updateProperties();
    });
}

void FeatureRecognitionViewModel::updateProperties()
{
    hasResults.set(myModel->hasResults());

    // Clear selection if no results
    if (!myModel->hasResults())
    {
        clearSelection();
    }
}
