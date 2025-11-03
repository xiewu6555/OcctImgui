#include "ImGuiView.h"
#include "mvvm/GlobalSettings.h"
#include "mvvm/MessageBus.h"
#include "utils/Logger.h"
#include "viewmodel/Commands.h"
#include "viewmodel/FeatureRecognitionViewModel.h"

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <nfd.h>
#include <filesystem>
#include <unordered_set>
#include <vector>

// 使用宏声明 ImGuiView 类的 logger
DECLARE_LOGGER(ImGuiView)

namespace
{
constexpr const char* ICON_EYE = "V";
constexpr const char* ICON_EYE_SLASH = "X";
}

ImGuiView::ImGuiView(std::shared_ptr<IViewModel> viewModel)
    : myViewModel(viewModel)
    , myWindow(nullptr)
{
    getImGuiViewLogger()->info("Creating view");
    subscribeToEvents();
}

ImGuiView::~ImGuiView()
{
    myConnections.disconnectAll();
}

std::shared_ptr<GeometryViewModel> ImGuiView::getGeometryViewModel() const
{
    return std::dynamic_pointer_cast<GeometryViewModel>(myViewModel);
}

void ImGuiView::initialize(GLFWwindow* window)
{
    LOG_FUNCTION_SCOPE(getImGuiViewLogger(), "initialize");
    getImGuiViewLogger()->info("Starting initialization");

    if (window == nullptr) {
        getImGuiViewLogger()->error("Initialization failed - window pointer is null");
        return;
    }
    myWindow = window;

    // 检查OpenGL上下文是否有效
    if (glfwGetCurrentContext() == nullptr) {
        getImGuiViewLogger()->error("Initialization failed - no valid OpenGL context");
        return;
    }

    try {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        // 移除Docking特性
        // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        // 配置字体以支持中文字符显示
        io.Fonts->AddFontDefault();

        ImFontConfig fontCfg;
        fontCfg.MergeMode = true;
        fontCfg.PixelSnapH = true;
        const ImWchar* chineseRanges = io.Fonts->GetGlyphRangesChineseFull();

        auto pathToUtf8 = [](const std::filesystem::path& p) -> std::string {
            const auto u8 = p.u8string();
#if defined(__cpp_char8_t)
            return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
#else
            return u8;
#endif
        };

        std::vector<std::filesystem::path> fontCandidates = {
#if defined(_WIN32)
            std::filesystem::path("C:/Windows/Fonts/msyh.ttc"),
            std::filesystem::path("C:/Windows/Fonts/msyh.ttf"),
            std::filesystem::path("C:/Windows/Fonts/simhei.ttf"),
            std::filesystem::path("C:/Windows/Fonts/simsun.ttc")
#else
            std::filesystem::path("/System/Library/Fonts/STHeiti Medium.ttc"),
            std::filesystem::path("/System/Library/Fonts/PingFang.ttc"),
            std::filesystem::path("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"),
            std::filesystem::path("/usr/share/fonts/truetype/arphic/ukai.ttc")
#endif
        };

        bool chineseFontLoaded = false;
        for (const auto& fontPath : fontCandidates) {
            if (!std::filesystem::exists(fontPath)) {
                continue;
            }

            std::string utf8Path = pathToUtf8(fontPath);
            if (io.Fonts->AddFontFromFileTTF(utf8Path.c_str(), 16.0f, &fontCfg, chineseRanges)) {
                chineseFontLoaded = true;
                getImGuiViewLogger()->info("Loaded ImGui font for CJK glyphs: {}", utf8Path);
                break;
            }
        }

        if (!chineseFontLoaded) {
            getImGuiViewLogger()->warn(
                "Failed to load CJK font; Chinese characters may not render correctly.");
        }

        // 设置ImGui风格
        ImGui::StyleColorsDark();

        // 初始化ImGui平台后端
        bool glfwInitSuccess = ImGui_ImplGlfw_InitForOpenGL(window, true);
        if (!glfwInitSuccess) {
            getImGuiViewLogger()->error("GLFW backend initialization failed");
            return;
        }

        // 初始化ImGui渲染器后端
        bool gl3InitSuccess = ImGui_ImplOpenGL3_Init("#version 330");
        if (!gl3InitSuccess) {
            getImGuiViewLogger()->error("OpenGL3 backend initialization failed");
            return;
        }

        getImGuiViewLogger()->info("Initialization completed successfully");
    }
    catch (const std::exception& e) {
        getImGuiViewLogger()->error("Exception during initialization: {}", e.what());
    }
    catch (...) {
        getImGuiViewLogger()->error("Unknown exception during initialization");
    }
}

void ImGuiView::newFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiView::render()
{
    // 渲染菜单栏
    renderMainMenu();

    // 渲染工具栏
    renderToolbar();

    popupContextMenu();

    // 渲染对象属性面板
    if (showObjectProperties) {
        renderObjectProperties();
    }

    // 渲染对象树面板
    if (showObjectTree) {
        renderObjectTree();
    }

    // 渲染状态栏
    renderStatusBar();

    // 渲染ImGui演示窗口（用于开发调试）
    if (showDemoWindow) {
        ImGui::ShowDemoWindow(&showDemoWindow);
    }

    // 渲染ImGui
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiView::shutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

bool ImGuiView::wantCaptureMouse() const
{
    return ImGui::GetIO().WantCaptureMouse;
}

void ImGuiView::renderMainMenu()
{
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Ctrl+N")) {
                // 处理新建命令
            }
            if (ImGui::MenuItem("Open", "Ctrl+O")) {
                // 处理打开命令
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                // 处理保存命令
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Import Model", "Ctrl+I")) {
                executeImportModel();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                // 处理退出命令
                glfwSetWindowShouldClose(myWindow, GLFW_TRUE);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Delete Selected",
                                "Delete",
                                false,
                                MVVM::SelectionManager::getInstance().hasSelection())) {
                executeDeleteSelected();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Object Properties", nullptr, &showObjectProperties);
            ImGui::MenuItem("Object Tree", nullptr, &showObjectTree);
            ImGui::Separator();
            ImGui::MenuItem("ImGui Demo Window", nullptr, &showDemoWindow);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Create")) {
            auto GeometryViewModel = getGeometryViewModel();

            if (GeometryViewModel) {
                if (ImGui::MenuItem("Box")) {
                    executeCreateBox();
                }
                if (ImGui::MenuItem("Cone")) {
                    executeCreateCone();
                }
                if (ImGui::MenuItem("Mesh")) {
                    executeCreateMesh();
                }
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void ImGuiView::renderToolbar()
{
    ImGui::Begin("Toolbar",
                 nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                     | ImGuiWindowFlags_NoScrollbar);

    auto GeometryViewModel = getGeometryViewModel();

    if (GeometryViewModel) {
        if (ImGui::Button("Import")) {
            executeImportModel();
        }
        ImGui::SameLine();
        if (ImGui::Button("Box")) {
            executeCreateBox();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cone")) {
            executeCreateCone();
        }
        ImGui::SameLine();
        if (ImGui::Button("Mesh")) {
            executeCreateMesh();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Delete", ImVec2(0, 0))) {
        executeDeleteSelected();
    }

    // Feature Recognition button
    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();
    if (ImGui::Button("Recognize Features")) {
        executeFeatureRecognition();
    }

    ImGui::End();
}

void ImGuiView::renderObjectProperties()
{
    ImGui::Begin("Object Properties", &showObjectProperties);

    auto GeometryViewModel = getGeometryViewModel();

    if (GeometryViewModel) {
        renderGeometryProperties();
    }
    else {
        ImGui::Text("Unknown view model type");
    }

    ImGui::End();
}

void ImGuiView::popupContextMenu()
{
    static bool wasPopupOpen = false;
    bool isPopupOpen = false;

    if (!wantCaptureMouse() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup("OcctView Context Menu");
    }

    if (ImGui::BeginPopup("OcctView Context Menu")) {
        isPopupOpen = true;
        
        auto GeometryViewModel = getGeometryViewModel();
        if (GeometryViewModel) {
            auto& globalSettings = GeometryViewModel->getGlobalSettings();
            
            // 显示模式选择
            if (ImGui::BeginMenu("Display Mode")) {
                int displayMode = globalSettings.displayMode.get();
                const char* displayModes[] = {"Shaded", "Wireframe", "Vertices"};
                
                for (int i = 0; i < IM_ARRAYSIZE(displayModes); i++) {
                    bool isSelected = (displayMode == i);
                    if (ImGui::MenuItem(displayModes[i], NULL, isSelected)) {
                        globalSettings.displayMode = i;
                    }
                }
                
                ImGui::EndMenu();
            }
            
            // Grid和ViewCube显示切换
            bool isGridVisible = globalSettings.isGridVisible.get();
            if (ImGui::MenuItem("Show Grid", NULL, isGridVisible)) {
                globalSettings.isGridVisible = !isGridVisible;
            }
            
            bool isViewCubeVisible = globalSettings.isViewCubeVisible.get();
            if (ImGui::MenuItem("Show View Cube", NULL, isViewCubeVisible)) {
                globalSettings.isViewCubeVisible = !isViewCubeVisible;
            }
        }

        ImGui::EndPopup();
    }

    // 检测菜单是否刚刚关闭
    if (wasPopupOpen && !isPopupOpen) {

        // 发送消息通知其他组件菜单已关闭
        MVVM::MessageBus::Message msg;
        msg.type = MVVM::MessageBus::MessageType::ViewChanged;
        msg.data = std::string("ImGuiContextMenuClosed");
        MVVM::MessageBus::getInstance().publish(msg);
    }

    wasPopupOpen = isPopupOpen;
}

void ImGuiView::renderGeometryProperties()
{
    auto GeometryViewModel = getGeometryViewModel();
    if (!GeometryViewModel)
        return;

    // 显示全局设置
    auto& globalSettings = GeometryViewModel->getGlobalSettings();

    if (MVVM::SelectionManager::getInstance().hasSelection()) {
        // 显示颜色选择器
        Quantity_Color currentColor = GeometryViewModel->getSelectedColor();
        float color[3] = {static_cast<float>(currentColor.Red()),
                          static_cast<float>(currentColor.Green()),
                          static_cast<float>(currentColor.Blue())};

        if (ImGui::ColorEdit3("Color", color)) {
            // 更新颜色
            Quantity_Color newColor(color[0], color[1], color[2], Quantity_TOC_RGB);
            GeometryViewModel->setSelectedColor(newColor);
        }
    }
    else {
        ImGui::Text("No objects selected");
    }
}

void ImGuiView::renderObjectTree()
{
    ImGui::Begin("Objects", &showObjectTree);

    auto GeometryViewModel = getGeometryViewModel();

    if (GeometryViewModel) {
        renderGeometryTree();
    }
    else {
        ImGui::Text("Unknown view model type");
    }

    ImGui::End();
}

void ImGuiView::renderGeometryTree()
{
    auto geometryViewModel = getGeometryViewModel();
    if (!geometryViewModel)
        return;

    auto model = geometryViewModel->getGeometryModel();
    if (!model) {
        ImGui::Text("No model available");
        return;
    }

    const auto entityIds = model->getAllEntityIds();

    ImGui::Text("Objects: %zu", entityIds.size());
    ImGui::Separator();

    if (entityIds.empty()) {
        ImGui::TextDisabled("No objects loaded");
        return;
    }

    // Cache currently selected object IDs for highlighting
    std::unordered_set<std::string> selectedIds;
    const auto& selectionInfo = MVVM::SelectionManager::getInstance().getCurrentSelection();
    selectedIds.reserve(selectionInfo.selectedObjects.size());
    for (const auto& entry : selectionInfo.selectedObjects) {
        std::string id = !entry.id.empty() ? entry.id : geometryViewModel->getObjectId(entry.object);
        if (!id.empty()) {
            selectedIds.insert(id);
        }
    }

    auto renderCategory = [&](const char* label, const std::vector<std::string>& ids) {
        if (ids.empty()) {
            return;
        }

        ImGuiTreeNodeFlags catFlags =
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (ImGui::TreeNodeEx(label, catFlags)) {
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4.0f, 2.0f));
            std::string tableId = std::string("##") + label;
            if (ImGui::BeginTable(tableId.c_str(),
                                  2,
                                  ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg
                                      | ImGuiTableFlags_NoBordersInBodyUntilResize)) {
                ImGui::TableSetupColumn("Object", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Visibility", ImGuiTableColumnFlags_WidthFixed, 28.0f);

                for (const auto& id : ids) {
                    ImGui::PushID(id.c_str());
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    bool isVisible = geometryViewModel->isObjectVisible(id);
                    bool isSelected = selectedIds.find(id) != selectedIds.end();

                    if (isSelected) {
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                               ImGui::GetColorU32(ImGuiCol_Header));
                    }

                    ImGui::AlignTextToFramePadding();
                    ImVec4 textColor = isVisible ? ImGui::GetStyleColorVec4(ImGuiCol_Text)
                                                 : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
                    ImGui::PushStyleColor(ImGuiCol_Text, textColor);

                    std::string labelText = "o " + id;
                    if (ImGui::Selectable(labelText.c_str(), isSelected)) {
                        bool append = ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift;
                        geometryViewModel->selectObject(id, append);
                    }
                    ImGui::PopStyleColor();

                    if (ImGui::BeginPopupContextItem("ObjectContext")) {
                        std::string visibilityLabel = isVisible ? "隐藏" : "显示";
                        if (ImGui::MenuItem(visibilityLabel.c_str())) {
                            bool newState = geometryViewModel->toggleObjectVisibility(id);
                            isVisible = newState;
                            if (!newState) {
                                selectedIds.erase(id);
                            }
                        }
                        ImGui::EndPopup();
                    }

                    ImGui::TableNextColumn();
                    const char* icon = isVisible ? ICON_EYE : ICON_EYE_SLASH;
                    if (ImGui::SmallButton(icon)) {
                        bool newState = geometryViewModel->toggleObjectVisibility(id);
                        isVisible = newState;
                        if (!newState) {
                            selectedIds.erase(id);
                        }
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", isVisible ? "点击隐藏" : "点击显示");
                    }

                    ImGui::PopID();
                }

                ImGui::EndTable();
            }
            ImGui::PopStyleVar();
            ImGui::TreePop();
        }
    };

    renderCategory("CAD Shapes",
                   model->getGeometryIdsByType(GeometryModel::GeometryType::SHAPE));
    renderCategory("Meshes", model->getGeometryIdsByType(GeometryModel::GeometryType::MESH));
}

void ImGuiView::renderStatusBar()
{
    const float height = ImGui::GetFrameHeight();
    const ImVec2 viewportSize = ImGui::GetMainViewport()->Size;

    ImGui::SetNextWindowPos(ImVec2(0, viewportSize.y - height));
    ImGui::SetNextWindowSize(ImVec2(viewportSize.x, height));

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs
        | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (ImGui::Begin("StatusBar", nullptr, windowFlags)) {
        ImGui::Text("OpenCascade ImGui Demo");
        ImGui::SameLine(ImGui::GetWindowWidth() - 120);

        ImGui::Text(mySelectionMessage.c_str());
    }
    ImGui::End();
}

void ImGuiView::executeCreateBox()
{
    auto GeometryViewModel = getGeometryViewModel();
    if (!GeometryViewModel)
        return;

    // 使用命令模式创建盒子
    Commands::CreateBoxCommand boxCmd(GeometryViewModel, gp_Pnt(0, 0, 0), 10, 10, 10);
    boxCmd.execute();
}

void ImGuiView::executeCreateCone()
{
    auto GeometryViewModel = getGeometryViewModel();
    if (!GeometryViewModel)
        return;

    // 使用命令模式创建圆锥
    Commands::CreateConeCommand coneCmd(GeometryViewModel, gp_Pnt(0, 0, 0), 5, 10);
    coneCmd.execute();
}

void ImGuiView::executeCreateMesh()
{
    auto GeometryViewModel = getGeometryViewModel();
    if (!GeometryViewModel)
        return;

    // 创建一个示例网格
    // 注意：这个方法需要在GeometryViewModel中实现
    GeometryViewModel->createMesh();
}

void ImGuiView::executeDeleteSelected()
{
    if (MVVM::SelectionManager::getInstance().hasSelection()) {
        // 使用命令模式删除选中对象
        Commands::DeleteSelectedCommand deleteCmd(myViewModel);
        deleteCmd.execute();
    }
}

void ImGuiView::executeImportModel()
{
    getImGuiViewLogger()->info("Executing import model command");

    // 初始化NFD (Native File Dialog)
    NFD_Init();

    nfdchar_t* outPath = nullptr;
    nfdfilteritem_t filterItems[4] = {{"All Files", "step,stp,stl,obj"},
                                      {"STEP Files", "step,stp"},
                                      {"STL Files", "stl"},
                                      {"OBJ Files", "obj"}};

    // 打开文件对话框
    nfdresult_t result = NFD_OpenDialog(&outPath, filterItems, 4, nullptr);

    if (result == NFD_OKAY) {
        getImGuiViewLogger()->info("Selected file: {}", outPath);

        // 获取GeometryViewModel
        auto GeometryViewModel = getGeometryViewModel();
        if (!GeometryViewModel) {
            getImGuiViewLogger()->error("Failed to get GeometryViewModel");
            NFD_FreePath(outPath);
            NFD_Quit();
            return;
        }

        // 创建并执行导入模型命令
        Commands::ImportModelCommand importCmd(GeometryViewModel, outPath);
        importCmd.execute();

        // 释放路径内存
        NFD_FreePath(outPath);
    }
    else if (result == NFD_CANCEL) {
        getImGuiViewLogger()->info("User canceled file dialog");
    }
    else {
        getImGuiViewLogger()->error("Error opening file dialog: {}", NFD_GetError());
    }

    // 清理NFD
    NFD_Quit();
}

void ImGuiView::executeFeatureRecognition()
{
    getImGuiViewLogger()->info("Executing feature recognition");

    if (!myFeatureRecognitionViewModel) {
        getImGuiViewLogger()->warn("Feature recognition viewmodel not set");
        return;
    }

    auto geometryVM = getGeometryViewModel();
    if (!geometryVM) {
        getImGuiViewLogger()->error("No geometry viewmodel available");
        return;
    }

    // Get selected shapes from SelectionManager
    const auto& selectionManager = MVVM::SelectionManager::getInstance();
    if (!selectionManager.hasSelection()) {
        getImGuiViewLogger()->info("No shape selected for feature recognition");
        return;
    }

    // Get the first selected shape
    TopoDS_Shape selectedShape = selectionManager.getSelectedShape();
    if (selectedShape.IsNull()) {
        getImGuiViewLogger()->error("Selected shape is null");
        return;
    }

    // Execute feature recognition
    // Use default parameters for now (can be configured later)
    std::string jsonParams = R"({
        "recognitionStrategy": "RuleBasedOnly",
        "operationType": "Milling",
        "linearTolerance": 0.01,
        "recognizeHoles": true,
        "recognizePockets": true,
        "recognizeSlots": true
    })";

    myFeatureRecognitionViewModel->executeRecognition(selectedShape, jsonParams);
}

void ImGuiView::subscribeToEvents()
{
    getImGuiViewLogger()->info("Subscribing to events");

    // 订阅 SelectionChanged 消息
    // Subscribe to selection changed events
    mySubcriptions = MVVM::MessageBus::getInstance().subscribeWithManager(
        MVVM::MessageBus::MessageType::SelectionChanged,
        [this](const MVVM::MessageBus::Message& message) {
            // Get the selection info
            try {
                const auto& selectionInfo = std::any_cast<MVVM::SelectionInfo>(message.data);
                // TODO  to string
            }
            catch (const std::bad_any_cast& e) {
                getImGuiViewLogger()->error("Failed to cast selection info: {}", e.what());
            }
        });
}
