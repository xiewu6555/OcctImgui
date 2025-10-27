/**
 * @file FeatureRecognitionView.cpp
 * @brief Implementation of FeatureRecognitionView
 */

#include "FeatureRecognitionView.h"
#include "utils/Logger.h"

#include <algorithm>
#include <sstream>

namespace
{
constexpr const char* ICON_EYE = "V";
constexpr const char* ICON_EYE_SLASH = "X";
}

//=============================================================================
// Constructor / Destructor
//=============================================================================
FeatureRecognitionView::FeatureRecognitionView(
    std::shared_ptr<FeatureRecognitionViewModel> viewModel)
    : myViewModel(viewModel)
{
    Utils::Logger::getLogger("View")->debug("FeatureRecognitionView created");
}

FeatureRecognitionView::~FeatureRecognitionView()
{
    Utils::Logger::getLogger("View")->debug("FeatureRecognitionView destroyed");
}

//=============================================================================
// IView Interface
//=============================================================================
void FeatureRecognitionView::initialize(GLFWwindow* window)
{
    myWindow = window;

    subscribeToViewModelEvents();
    subscribeToMessageBus();

    Utils::Logger::getLogger("View")->info("FeatureRecognitionView initialized");
}

void FeatureRecognitionView::newFrame()
{
    // Nothing to do per frame
}

void FeatureRecognitionView::render()
{
    if (!myShowFeatureTree)
    {
        return;
    }

    renderFeaturePanel();
}

void FeatureRecognitionView::shutdown()
{
    myConnections.disconnectAll();
    Utils::Logger::getLogger("View")->info("FeatureRecognitionView shut down");
}

bool FeatureRecognitionView::wantCaptureMouse() const
{
    return myShowFeatureTree && ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
}

//=============================================================================
// UI Rendering
//=============================================================================
void FeatureRecognitionView::renderFeaturePanel()
{
    ImGui::SetNextWindowSize(ImVec2(350, 600), ImGuiCond_FirstUseEver);
    ImGui::Begin("Recognized Features", &myShowFeatureTree);

    // Toolbar
    if (ImGui::Button("Clear"))
    {
        myViewModel->clearResults();
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    ImGui::InputText("##Filter", myFilterText, sizeof(myFilterText));
    ImGui::SameLine();
    ImGui::TextDisabled("(Filter)");

    ImGui::Separator();

    // Feature tree
    renderFeatureTree();

    ImGui::End();

    // Status bar (optional, at bottom of main window)
    if (myShowStatus)
    {
        renderStatus();
    }
}

void FeatureRecognitionView::renderFeatureTree()
{
    if (!myViewModel->hasResults.get())
    {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No features recognized yet");
        return;
    }

    const auto& groups = myViewModel->getFeatureGroups();

    ImGui::BeginChild("FeatureTreeScroll",
                      ImVec2(0, 0),
                      false,
                      ImGuiWindowFlags_HorizontalScrollbar);

    for (size_t i = 0; i < groups.size(); ++i)
    {
        const auto& group = groups[i];

        // Skip if doesn't match filter
        if (strlen(myFilterText) > 0 && !matchesFilter(group.name))
        {
            continue;
        }

        renderFeatureGroup(group, static_cast<int>(i));
    }

    ImGui::EndChild();
}

void FeatureRecognitionView::renderFeatureGroup(
    const FeatureRecognitionModel::FeatureGroup& group,
    int groupIdx)
{
    auto logger = Utils::Logger::getLogger("View");

    const bool groupVisible = myViewModel->isGroupVisible(groupIdx);
    ImVec4 baseColor = toImGuiColor(group.color);
    ImVec4 textColor = applyVisibilityTint(baseColor, groupVisible);

    // Build node label with color indicator
    std::ostringstream labelStream;
    labelStream << group.name;

    ImGui::PushStyleColor(ImGuiCol_Text, textColor);

    // Tree node flags
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

    // Highlight if selected
    if (myViewModel->selectedGroupIndex.get() == groupIdx)
    {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    bool isSelected = myViewModel->selectedGroupIndex.get() == groupIdx;
    if (isSelected)
    {
        ImGui::SetNextItemOpen(true);
    }

    bool nodeOpen =
        ImGui::TreeNodeEx((void*)(intptr_t)groupIdx, flags, "%s", labelStream.str().c_str());

    bool nodeClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

    ImGui::PopStyleColor();

    if (nodeClicked)
    {
        myViewModel->selectFeature(groupIdx, -1, -1);
    }

    // Count badge
    ImGui::SameLine();
    renderColoredBadge(std::to_string(group.totalGroupFeatureCount), textColor);

    // Visibility toggle icon
    ImGui::SameLine();
    std::string visIcon = groupVisible ? ICON_EYE : ICON_EYE_SLASH;  // Using placeholder
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    if (ImGui::SmallButton((visIcon + "##vis" + std::to_string(groupIdx)).c_str()))
    {
        myViewModel->toggleFeatureGroupVisibility(groupIdx);
    }
    ImGui::PopStyleColor();

    // Render children if node is open
    if (nodeOpen)
    {
        // Check if group has subGroups
        if (group.subGroups.has_value())
        {
            const auto& subGroups = group.subGroups.value();
            for (size_t j = 0; j < subGroups.size(); ++j)
            {
                renderSubGroup(subGroups[j], group, groupIdx, static_cast<int>(j));
            }
        }
        // Otherwise render direct features
        else if (group.features.has_value())
        {
            const auto& features = group.features.value();
            for (size_t k = 0; k < features.size(); ++k)
            {
                renderFeature(features[k], static_cast<int>(k), groupIdx, -1, group.color);
            }
        }

        ImGui::TreePop();
    }
}

void FeatureRecognitionView::renderSubGroup(
    const FeatureRecognitionModel::SubGroup& subGroup,
    const FeatureRecognitionModel::FeatureGroup& group,
    int groupIdx,
    int subGroupIdx)
{
    // Build label from parameters
    std::ostringstream labelStream;
    labelStream << group.name.substr(0, group.name.find("("));  // Remove trailing "(s)"

    if (!subGroup.parameters.empty())
    {
        labelStream << " (";
        for (size_t i = 0; i < subGroup.parameters.size() && i < 3; ++i)  // Show first 3 params
        {
            if (i > 0)
                labelStream << ", ";
            labelStream << subGroup.parameters[i].value;
        }
        labelStream << ")";
    }

    const bool groupVisible = myViewModel->isGroupVisible(groupIdx);
    ImVec4 color = toImGuiColor(group.color);
    ImVec4 textColor = applyVisibilityTint(color, groupVisible);

    ImGui::PushStyleColor(ImGuiCol_Text, textColor);

    // Tree node flags
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

    // Highlight if selected
    if (myViewModel->selectedGroupIndex.get() == groupIdx &&
        myViewModel->selectedSubGroupIndex.get() == subGroupIdx)
    {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    void* nodeId = (void*)(intptr_t)(groupIdx * 1000 + subGroupIdx + 100);
    bool isSelected = myViewModel->selectedGroupIndex.get() == groupIdx
                      && myViewModel->selectedSubGroupIndex.get() == subGroupIdx;
    if (isSelected)
    {
        ImGui::SetNextItemOpen(true);
    }

    bool nodeOpen = ImGui::TreeNodeEx(nodeId, flags, "%s", labelStream.str().c_str());
    bool nodeClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

    ImGui::PopStyleColor();

    if (nodeClicked)
    {
        myViewModel->selectFeature(groupIdx, subGroupIdx, -1);
    }

    // Count badge
    ImGui::SameLine();
    renderColoredBadge(std::to_string(subGroup.featureCount), textColor);

    // Render children if node is open
    if (nodeOpen)
    {
        // Show parameters
        if (!subGroup.parameters.empty())
        {
            renderParameters(subGroup.parameters);
        }

        // Show individual features
        for (size_t k = 0; k < subGroup.features.size(); ++k)
        {
            renderFeature(subGroup.features[k], static_cast<int>(k), groupIdx, subGroupIdx, group.color);
        }

        ImGui::TreePop();
    }
}

void FeatureRecognitionView::renderFeature(
    const FeatureRecognitionModel::Feature& feature,
    int featureIdx,
    int groupIdx,
    int subGroupIdx,
    const Quantity_Color& color)
{
    std::ostringstream labelStream;
    labelStream << "Feature " << (featureIdx + 1);

    const bool groupVisible = myViewModel->isGroupVisible(groupIdx);
    ImVec4 imColor = toImGuiColor(color);
    ImVec4 textColor = applyVisibilityTint(imColor, groupVisible);

    ImGui::PushStyleColor(ImGuiCol_Text, textColor);

    // Tree node flags
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    // Highlight if selected
    if (myViewModel->selectedGroupIndex.get() == groupIdx &&
        myViewModel->selectedSubGroupIndex.get() == subGroupIdx &&
        myViewModel->selectedFeatureIndex.get() == featureIdx)
    {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    void* nodeId = (void*)(intptr_t)(groupIdx * 10000 + (subGroupIdx + 1) * 100 + featureIdx);
    ImGui::TreeNodeEx(nodeId, flags, "%s", labelStream.str().c_str());

    ImGui::PopStyleColor();

    // Handle selection
    if (ImGui::IsItemClicked())
    {
        myViewModel->selectFeature(groupIdx, subGroupIdx, featureIdx);
    }
    if (myViewModel->selectedGroupIndex.get() == groupIdx
        && myViewModel->selectedSubGroupIndex.get() == subGroupIdx
        && myViewModel->selectedFeatureIndex.get() == featureIdx)
    {
        ImGui::SetScrollHereY();
    }

    // Tooltip with face IDs
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Faces: ");
        for (const auto& shapeID : feature.shapeIDs)
        {
            ImGui::SameLine();
            ImGui::Text("%s", shapeID.id.c_str());
        }
        ImGui::EndTooltip();
    }
}

void FeatureRecognitionView::renderParameters(
    const std::vector<FeatureRecognitionModel::Parameter>& parameters)
{
    ImGui::Indent();

    for (const auto& param : parameters)
    {
        std::string label = param.name + ": ";
        std::string value = formatParameterValue(param);

        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", label.c_str());
        ImGui::SameLine();
        ImGui::Text("%s", value.c_str());
    }

    ImGui::Unindent();
}

void FeatureRecognitionView::renderStatus()
{
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, 25), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetIO().DisplaySize.y - 25), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin("##FeatureStatusBar",
                 nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

    std::string status = myViewModel->statusMessage.get();
    if (!status.empty())
    {
        ImGui::Text("%s", status.c_str());
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

//=============================================================================
// Helper Methods
//=============================================================================
ImVec4 FeatureRecognitionView::toImGuiColor(const Quantity_Color& color) const
{
    return ImVec4(static_cast<float>(color.Red()),
                  static_cast<float>(color.Green()),
                  static_cast<float>(color.Blue()),
                  1.0f);
}

ImVec4 FeatureRecognitionView::applyVisibilityTint(const ImVec4& color, bool isVisible) const
{
    if (isVisible)
    {
        return color;
    }

    ImVec4 disabled = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    return ImVec4((color.x + disabled.x) * 0.5f,
                  (color.y + disabled.y) * 0.5f,
                  (color.z + disabled.z) * 0.5f,
                  color.w);
}

void FeatureRecognitionView::renderColoredBadge(const std::string& text, const ImVec4& color)
{
    ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
    ImVec2 padding(6.0f, 2.0f);
    ImVec2 badgeSize(textSize.x + padding.x * 2, textSize.y + padding.y * 2);

    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Draw rounded rectangle background
    ImU32 bgColor = ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, 0.3f));
    drawList->AddRectFilled(cursorPos,
                            ImVec2(cursorPos.x + badgeSize.x, cursorPos.y + badgeSize.y),
                            bgColor,
                            4.0f);  // Rounded corners

    // Draw border
    ImU32 borderColor = ImGui::ColorConvertFloat4ToU32(color);
    drawList->AddRect(cursorPos,
                      ImVec2(cursorPos.x + badgeSize.x, cursorPos.y + badgeSize.y),
                      borderColor,
                      4.0f,
                      0,
                      1.5f);  // Border thickness

    // Draw text
    ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + padding.x, cursorPos.y + padding.y));
    ImGui::TextColored(color, "%s", text.c_str());

    // Advance cursor
    ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + badgeSize.x + 5.0f, cursorPos.y));
    ImGui::Dummy(ImVec2(0, badgeSize.y));
}

std::string FeatureRecognitionView::formatParameterValue(
    const FeatureRecognitionModel::Parameter& param) const
{
    std::ostringstream oss;
    oss << param.value;
    if (!param.units.empty())
    {
        oss << " " << param.units;
    }
    return oss.str();
}

bool FeatureRecognitionView::matchesFilter(const std::string& text) const
{
    if (strlen(myFilterText) == 0)
    {
        return true;
    }

    std::string lowerText = text;
    std::string lowerFilter = myFilterText;

    std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::tolower);
    std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);

    return lowerText.find(lowerFilter) != std::string::npos;
}

//=============================================================================
// Event Subscriptions
//=============================================================================
void FeatureRecognitionView::subscribeToViewModelEvents()
{
    // Subscribe to recognition events
    myConnections.track(myViewModel->onRecognitionCompleted.connect([this]() {
        auto logger = Utils::Logger::getLogger("View");
        logger->info("Recognition completed, updating view");
    }));

    myConnections.track(myViewModel->onRecognitionFailed.connect([this](const std::string& error) {
        auto logger = Utils::Logger::getLogger("View");
        logger->error("Recognition failed: {}", error);
    }));

    // Subscribe to property changes
    myConnections.track(
        myViewModel->hasResults.valueChanged.connect([this](const bool&, const bool& hasResults) {
            auto logger = Utils::Logger::getLogger("View");
            logger->debug("hasResults changed to: {}", hasResults);
        }));
}

void FeatureRecognitionView::subscribeToMessageBus()
{
    // Subscribe to relevant messages if needed
    // Currently not needed, but can be added later
}

