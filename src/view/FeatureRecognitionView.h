/**
 * @file FeatureRecognitionView.h
 * @brief View for feature recognition visualization
 *
 * This view provides a tree-based UI for displaying and interacting with
 * recognized manufacturing features. It shows feature groups, sub-groups,
 * and individual features with their parameters.
 */
#pragma once

#include "IView.h"
#include "mvvm/MessageBus.h"
#include "viewmodel/FeatureRecognitionViewModel.h"

#include <imgui.h>
#include <memory>
#include <optional>
#include <string>

/**
 * @class FeatureRecognitionView
 * @brief ImGui-based view for feature recognition
 *
 * Features:
 * - Tree-based display of recognized features
 * - Color-coded feature types
 * - Interactive selection
 * - Visibility toggles
 * - Parameter display
 */
class FeatureRecognitionView: public IView
{
public:
    /**
     * @brief Constructor
     * @param viewModel The feature recognition view model
     */
    explicit FeatureRecognitionView(std::shared_ptr<FeatureRecognitionViewModel> viewModel);

    /**
     * @brief Destructor
     */
    ~FeatureRecognitionView() override;

    // ========== IView Interface ==========

    /**
     * @brief Initializes the view
     * @param window The GLFW window
     */
    void initialize(GLFWwindow* window) override;

    /**
     * @brief Starts a new frame
     */
    void newFrame() override;

    /**
     * @brief Renders the view
     */
    void render() override;

    /**
     * @brief Shuts down the view
     */
    void shutdown() override;

    /**
     * @brief Checks if mouse input should be captured
     * @return true if mouse is over the view, false otherwise
     */
    bool wantCaptureMouse() const override;

    /**
     * @brief Gets the associated view model
     * @return Shared pointer to the view model
     */
    std::shared_ptr<IViewModel> getViewModel() const override
    {
        return myViewModel;
    }

private:
    std::shared_ptr<FeatureRecognitionViewModel> myViewModel;
    GLFWwindow* myWindow = nullptr;

    /** Connection tracker for signal connections */
    MVVM::ConnectionTracker myConnections;

    /** Subscriptions to message bus events */
    MVVM::Subscription mySubscriptions;

    // ========== UI State ==========

    /** Whether the feature tree panel is visible */
    bool myShowFeatureTree = true;

    /** Whether the status panel is visible */
    bool myShowStatus = true;

    /** Search/filter text */
    char myFilterText[256] = "";

    struct SelectionCoordinates
    {
        int group = -1;
        int subGroup = -1;
        int feature = -1;

        bool matches(int groupIdx, int subGroupIdx, int featureIdx) const
        {
            return group == groupIdx && subGroup == subGroupIdx && feature == featureIdx;
        }
    };

    /** Pending scroll target to ensure the latest selection is visible */
    std::optional<SelectionCoordinates> myPendingScrollSelection;

    // ========== UI Rendering Methods ==========

    /**
     * @brief Renders the main feature recognition panel
     */
    void renderFeaturePanel();

    /**
     * @brief Renders the feature tree
     */
    void renderFeatureTree();

    /**
     * @brief Renders a single feature group node
     * @param group The feature group
     * @param groupIdx Index of the group
     */
    void renderFeatureGroup(const FeatureRecognitionModel::FeatureGroup& group, int groupIdx);

    /**
     * @brief Renders a sub-group node
     * @param subGroup The sub-group
     * @param group The parent feature group
     * @param groupIdx Index of the group
     * @param subGroupIdx Index of the sub-group
     */
    void renderSubGroup(const FeatureRecognitionModel::SubGroup& subGroup,
                        const FeatureRecognitionModel::FeatureGroup& group,
                        int groupIdx,
                        int subGroupIdx);

    /**
     * @brief Renders a feature node
     * @param feature The feature
     * @param featureIdx Index of the feature
     * @param groupIdx Index of the group
     * @param subGroupIdx Index of the sub-group (-1 for direct features)
     * @param color The color to use for highlighting
     */
    void renderFeature(const FeatureRecognitionModel::Feature& feature,
                       int featureIdx,
                       int groupIdx,
                       int subGroupIdx,
                       const Quantity_Color& color);

    /**
     * @brief Renders parameters for a sub-group
     * @param parameters The parameters to display
     */
    void renderParameters(const std::vector<FeatureRecognitionModel::Parameter>& parameters);

    /**
     * @brief Renders the status bar
     */
    void renderStatus();

    // ========== Helper Methods ==========

    /**
     * @brief Converts Quantity_Color to ImVec4
     * @param color The OCC color
     * @return ImGui color
     */
    ImVec4 toImGuiColor(const Quantity_Color& color) const;

    /**
     * @brief Applies a disabled tint when the owning group is hidden
     */
    ImVec4 applyVisibilityTint(const ImVec4& color, bool isVisible) const;

    /**
     * @brief Renders a colored badge with text
     * @param text The badge text
     * @param color The badge color
     */
    void renderColoredBadge(const std::string& text, const ImVec4& color);

    /**
     * @brief Formats parameter value for display
     * @param param The parameter
     * @return Formatted string
     */
    std::string formatParameterValue(const FeatureRecognitionModel::Parameter& param) const;

    /**
     * @brief Checks if a group/feature matches the filter
     * @param text The text to check
     * @return true if matches, false otherwise
     */
    bool matchesFilter(const std::string& text) const;

    /**
     * @brief Queue a scroll request for the given selection
     */
    void queueScrollToSelection(int groupIdx, int subGroupIdx, int featureIdx);

    /**
     * @brief Consume a pending scroll request if it targets the current node
     */
    bool consumeScrollRequest(int groupIdx, int subGroupIdx, int featureIdx);

    // ========== Event Subscriptions ==========

    /**
     * @brief Subscribes to ViewModel signals
     */
    void subscribeToViewModelEvents();

    /**
     * @brief Subscribes to MessageBus events
     */
    void subscribeToMessageBus();
};
