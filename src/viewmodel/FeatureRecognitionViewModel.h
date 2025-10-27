/**
 * @file FeatureRecognitionViewModel.h
 * @brief ViewModel for feature recognition functionality
 *
 * This ViewModel manages the interaction between the FeatureRecognitionModel
 * and the FeatureRecognitionView, exposing properties and commands.
 */
#pragma once

#include "IViewModel.h"
#include "model/FeatureRecognitionModel.h"
#include "mvvm/Property.h"
#include "mvvm/Signal.h"

#include <memory>
#include <string>
#include <vector>

/**
 * @class FeatureRecognitionViewModel
 * @brief ViewModel for feature recognition
 *
 * This class provides:
 * - Properties for UI state (recognition status, selection, etc.)
 * - Commands for triggering recognition and handling selection
 * - Access to feature data for visualization
 */
class FeatureRecognitionViewModel: public IViewModel
{
public:
    /**
     * @brief Constructor
     * @param model The feature recognition model
     */
    explicit FeatureRecognitionViewModel(std::shared_ptr<FeatureRecognitionModel> model);

    /**
     * @brief Destructor
     */
    ~FeatureRecognitionViewModel() override = default;

    // ========== IViewModel Interface ==========

    /**
     * @brief Gets the underlying model
     * @return Shared pointer to the model
     */
    std::shared_ptr<IModel> getModel() const override
    {
        return myModel;
    }

    /**
     * @brief Deletes selected objects (not implemented for feature recognition)
     */
    void deleteSelectedObjects() override
    {
        // Feature recognition doesn't support object deletion
    }

    /**
     * @brief Gets the AIS context (returns null for feature recognition)
     * @return Null handle as feature recognition doesn't manage its own context
     */
    Handle(AIS_InteractiveContext) getContext() const override
    {
        return Handle(AIS_InteractiveContext)();
    }

    // ========== Properties ==========

    /** Whether recognition results are available */
    MVVM::Property<bool> hasResults{false};

    /** Current status message */
    MVVM::Property<std::string> statusMessage{""};

    /** Index of selected feature group (-1 = none) */
    MVVM::Property<int> selectedGroupIndex{-1};

    /** Index of selected sub-group (-1 = none or direct features) */
    MVVM::Property<int> selectedSubGroupIndex{-1};

    /** Index of selected feature (-1 = none) */
    MVVM::Property<int> selectedFeatureIndex{-1};

    /** Whether recognition is in progress */
    MVVM::Property<bool> isRecognizing{false};

    // ========== Signals ==========

    /** Emitted when recognition starts */
    MVVM::Signal<> onRecognitionStarted;

    /** Emitted when recognition completes successfully */
    MVVM::Signal<> onRecognitionCompleted;

    /** Emitted when recognition fails */
    MVVM::Signal<const std::string&> onRecognitionFailed;

    /** Emitted when feature selection changes (groupIdx, subGroupIdx, featureIdx) */
    MVVM::Signal<int, int, int> onFeatureSelected;

    /** Emitted when feature visibility toggles (groupIdx, visible) */
    MVVM::Signal<int, bool> onFeatureVisibilityChanged;

    // ========== Commands ==========

    /**
     * @brief Executes feature recognition on a shape
     * @param shape The TopoDS_Shape to recognize
     * @param jsonParams Optional JSON parameters for recognition
     */
    void executeRecognition(const TopoDS_Shape& shape, const std::string& jsonParams = "");

    /**
     * @brief Loads recognition results from JSON string
     * @param jsonString The JSON result string
     */
    void loadResultsFromJson(const std::string& jsonString);

    /**
     * @brief Selects a feature
     * @param groupIdx Index of the feature group
     * @param subGroupIdx Index of the sub-group (-1 for direct features)
     * @param featureIdx Index of the feature
     */
    void selectFeature(int groupIdx, int subGroupIdx, int featureIdx);

    /**
     * @brief Toggles visibility of a feature group
     * @param groupIdx Index of the feature group
     */
    void toggleFeatureGroupVisibility(int groupIdx);

    /**
     * @brief Clears current selection
     */
    void clearSelection();

    /**
     * @brief Clears all recognition results
     */
    void clearResults();

    // ========== Query Methods ==========

    /**
     * @brief Gets all feature groups
     * @return Vector of feature groups
     */
    const std::vector<FeatureRecognitionModel::FeatureGroup>& getFeatureGroups() const;

    /**
     * @brief Gets face IDs for the currently selected feature
     * @return Vector of face ID strings
     */
    std::vector<std::string> getSelectedFeatureFaceIDs() const;

    /**
     * @brief Gets face IDs for a specific feature
     * @param groupIdx Index of the feature group
     * @param subGroupIdx Index of the sub-group (-1 for direct features)
     * @param featureIdx Index of the feature
     * @return Vector of face ID strings
     */
    std::vector<std::string> getFeatureFaceIDs(int groupIdx, int subGroupIdx, int featureIdx) const;

    /**
     * @brief Gets the color for a feature group
     * @param groupIdx Index of the feature group
     * @return The Quantity_Color, or default gray if not found
     */
    Quantity_Color getFeatureGroupColor(int groupIdx) const;

    /**
     * @brief Returns whether a feature group is visible
     */
    bool isGroupVisible(int groupIdx) const;

    /**
     * @brief 根据面 ID 查找所在的特征位置
     */
    std::vector<FeatureRecognitionModel::FeatureLocation>
        findFeatureLocationsForFace(const std::string& faceId) const;

    /**
     * @brief Gets the FeatureRecognitionModel
     * @return Shared pointer to the model
     */
    std::shared_ptr<FeatureRecognitionModel> getFeatureModel() const
    {
        return myModel;
    }

private:
    std::shared_ptr<FeatureRecognitionModel> myModel;  // The feature recognition model

    /**
     * @brief Subscribes to model change events
     */
    void subscribeToModelEvents();

    /**
     * @brief Updates UI properties based on model state
     */
    void updateProperties();
};
