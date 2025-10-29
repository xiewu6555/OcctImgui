#include "OcctView.h"
#include "mvvm/GlobalSettings.h"
#include "mvvm/MessageBus.h"
#include "mvvm/SelectionManager.h"
#include "utils/Logger.h"
#include "viewmodel/FeatureRecognitionViewModel.h"
#include "model/FeatureRecognitionModel.h"

#include <AIS_ColoredShape.hxx>
#include <AIS_Shape.hxx>
#include <AIS_ViewCube.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <BRep_Builder.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Aspect_Handle.hxx>
#include <GLFW/glfw3.h>
#include <Message.hxx>
#include <Message_Messenger.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>
#include <Prs3d_LineAspect.hxx>
#include <Graphic3d_NameOfMaterial.hxx>

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>
#include <vector>

// 使用宏声明 OcctView 类的 logger
DECLARE_LOGGER(OcctView)

// 辅助函数，转换GLFW鼠标按键为OCCT按键
namespace
{
//! Convert GLFW mouse button into Aspect_VKeyMouse.
static Aspect_VKeyMouse mouseButtonFromGlfw(int theButton)
{
    switch (theButton) {
        case GLFW_MOUSE_BUTTON_LEFT:
            return Aspect_VKeyMouse_LeftButton;
        case GLFW_MOUSE_BUTTON_RIGHT:
            return Aspect_VKeyMouse_RightButton;
        case GLFW_MOUSE_BUTTON_MIDDLE:
            return Aspect_VKeyMouse_MiddleButton;
    }
    return Aspect_VKeyMouse_NONE;
}

//! Convert GLFW key modifiers into Aspect_VKeyFlags.
static Aspect_VKeyFlags keyFlagsFromGlfw(int theFlags)
{
    Aspect_VKeyFlags aFlags = Aspect_VKeyFlags_NONE;
    if ((theFlags & GLFW_MOD_SHIFT) != 0) {
        aFlags |= Aspect_VKeyFlags_SHIFT;
    }
    if ((theFlags & GLFW_MOD_CONTROL) != 0) {
        aFlags |= Aspect_VKeyFlags_CTRL;
    }
    if ((theFlags & GLFW_MOD_ALT) != 0) {
        aFlags |= Aspect_VKeyFlags_ALT;
    }
    if ((theFlags & GLFW_MOD_SUPER) != 0) {
        aFlags |= Aspect_VKeyFlags_META;
    }
    return aFlags;
}

constexpr int LEFT_CLICK_DRAG_THRESHOLD = 3;
}  // namespace

OcctView::OcctView(std::shared_ptr<GeometryViewModel> viewModel, Handle(GlfwOcctWindow) window)
    : myViewModel(viewModel)
    , myWindow(window)
{
    getOcctViewLogger()->info("Creating view");
    subscribeToEvents();
}

OcctView::~OcctView()
{
    getOcctViewLogger()->info("Cleaning up view");

    // Disconnect all signal connections
    myConnections.disconnectAll();

    // Clean up resources
    cleanup();
}

void OcctView::cleanup()
{
    getOcctViewLogger()->info("Cleaning up view");
    clearFeatureHighlights();
    clearFeatureOverview();
    if (!myView.IsNull()) {
        myView->Remove();
    }
}

void OcctView::initialize()
{
    LOG_FUNCTION_SCOPE(getOcctViewLogger(), "initialize");
    getOcctViewLogger()->info("Starting initialization");
    if (myWindow.IsNull() || myWindow->getGlfwWindow() == nullptr) {
        getOcctViewLogger()->error("Initialization failed - invalid window");
        return;
    }

    try {
        // 检查OpenGL上下文
        if (glfwGetCurrentContext() == nullptr) {
            getOcctViewLogger()->error("Initialization failed - no current OpenGL context");
            return;
        }

        // 创建图形驱动
        Handle(OpenGl_GraphicDriver) aGraphicDriver =
            new OpenGl_GraphicDriver(myWindow->GetDisplay(), Standard_False);
        aGraphicDriver->SetBuffersNoSwap(Standard_True);
        getOcctViewLogger()->info("OCCT: OpenGL graphic driver created, BuffersNoSwap=True");

        // 创建3D查看器
        Handle(V3d_Viewer) aViewer = myViewModel->getViewer();
        aViewer->SetDefaultLights();
        aViewer->SetLightOn();
        aViewer->SetDefaultTypeOfView(V3d_PERSPECTIVE);
        aViewer->ActivateGrid(Aspect_GT_Rectangular, Aspect_GDM_Lines);
        getOcctViewLogger()->info("OCCT: V3d_Viewer configured");

        // 创建视图
        myView = aViewer->CreateView();
        if (myView.IsNull()) {
            getOcctViewLogger()->error("OCCT: Failed to create view");
            return;
        }

        myView->SetWindow(myWindow, myWindow->NativeGlContext());
        myView->Window()->DoResize();
        myView->ChangeRenderingParams().ToShowStats = Standard_True;
        getOcctViewLogger()->info("OCCT: V3d_View created and configured");

        // 显示视图
        myWindow->Map();
        getOcctViewLogger()->info("OCCT: Window mapped");

        // 设置视图组件
        setupViewCube();
        setupGrid();
        getOcctViewLogger()->info("OCCT: View components setup complete");

        // 应用初始设置
        updateVisibility();

        // 输出OpenGL信息
        TCollection_AsciiString aGlInfo;
        TColStd_IndexedDataMapOfStringString aRendInfo;
        myView->DiagnosticInformation(aRendInfo, Graphic3d_DiagnosticInfo_Basic);
        for (TColStd_IndexedDataMapOfStringString::Iterator aValueIter(aRendInfo);
             aValueIter.More();
             aValueIter.Next()) {
            getOcctViewLogger()->info("OCCT OpenGL: {} = {}",
                                      aValueIter.Key().ToCString(),
                                      aValueIter.Value().ToCString());
        }

        getOcctViewLogger()->info("OCCT: Initialization complete");
    }
    catch (const std::exception& e) {
        getOcctViewLogger()->error("OCCT: Initialization exception: {}", e.what());
    }
    catch (...) {
        getOcctViewLogger()->error("OCCT: Unknown exception during initialization");
    }
}

void OcctView::render()
{
    if (myView.IsNull() || myViewModel->getContext().IsNull()) {
        getOcctViewLogger()->warn("OCCT: Render skipped - view or context is null");
        return;
    }

    try {
        // 立即更新视图
        myView->InvalidateImmediate();

        // 刷新视图事件
        FlushViewEvents(myViewModel->getContext(), myView, Standard_True);
    }
    catch (const std::exception& e) {
        getOcctViewLogger()->error("OCCT: Render exception: {}", e.what());
    }
    catch (...) {
        getOcctViewLogger()->error("OCCT: Unknown exception during render");
    }
}

void OcctView::onMouseMove(int posX, int posY)
{
    if (myView.IsNull()) {
        return;
    }

    // 当弹出菜单关闭时，需要重置视图输入，否则鼠标移动会导致视图的缩放，
    // 但也许有更好的方法来处理这个问题
    if (myResetViewInput) {
        myResetViewInput = false;
        ResetViewInput();
        getOcctViewLogger()->debug("Resetting view input after context menu close");
        return;
    }

    const Graphic3d_Vec2i aNewPos(posX, posY);

    if (myLeftButtonPressed && !myLeftButtonDragDetected) {
        if (std::abs(posX - myLeftButtonPressPosX) > LEFT_CLICK_DRAG_THRESHOLD
            || std::abs(posY - myLeftButtonPressPosY) > LEFT_CLICK_DRAG_THRESHOLD) {
            myLeftButtonDragDetected = true;
        }
    }

    UpdateMousePosition(aNewPos, PressedMouseButtons(), LastMouseFlags(), Standard_False);
}

void OcctView::onMouseButton(int button, int action, int mods)
{
    auto logger = getOcctViewLogger();
    logger->debug("Mouse button: {}, action: {}, mods: {}", button, action, mods);

    if (myView.IsNull()) {
        return;
    }

    const Graphic3d_Vec2i aPos = myWindow->CursorPosition();
    ImGuiIO&              io   = ImGui::GetIO();
    bool                  wantCapture = io.WantCaptureMouse;

    // Handle OCCT view control
    if (action == GLFW_PRESS) {
        PressMouseButton(aPos, mouseButtonFromGlfw(button), keyFlagsFromGlfw(mods), false);

        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            myLeftButtonPressed = true;
            myLeftButtonDragDetected = false;
            myLeftButtonPressPosX = aPos.x();
            myLeftButtonPressPosY = aPos.y();
        }

        // Right click to clear selection
        if (button == GLFW_MOUSE_BUTTON_RIGHT && (mods & GLFW_MOD_CONTROL) == 0 && !wantCapture) {
            // Popup a context menu
            getOcctViewLogger()->info("Clearing selection");
            MVVM::SelectionManager::getInstance().clearSelection();
            myViewModel->getContext()->ClearSelected(Standard_True);
        }
    }
    else if (action == GLFW_RELEASE) {
        ReleaseMouseButton(aPos, mouseButtonFromGlfw(button), keyFlagsFromGlfw(mods), false);

        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            bool shouldHandleSelection =
                !wantCapture && (mods & GLFW_MOD_CONTROL) == 0 && !myLeftButtonDragDetected;

            if (shouldHandleSelection) {
                handleSelection(aPos.x(), aPos.y());
            }

            myLeftButtonPressed = false;
            myLeftButtonDragDetected = false;
        }
    }
}

void OcctView::onMouseScroll(double offsetX, double offsetY)
{
    if (myView.IsNull()) {
        return;
    }

    UpdateZoom(Aspect_ScrollDelta(myWindow->CursorPosition(), int(offsetY * 8.0)));
}

void OcctView::onResize(int width, int height)
{
    if (width != 0 && height != 0 && !myView.IsNull()) {
        myView->Window()->DoResize();
        myView->MustBeResized();
        myView->Invalidate();
        FlushViewEvents(myViewModel->getContext(), myView, true);
    }
}

void OcctView::handleViewRedraw(const Handle(AIS_InteractiveContext) & theCtx,
                                const Handle(V3d_View) & theView)
{
    AIS_ViewController::handleViewRedraw(theCtx, theView);
    myToWaitEvents = !myToAskNextFrame;
}

void OcctView::setupViewCube()
{
    myViewCube = new AIS_ViewCube();
    myViewCube->SetSize(55);
    myViewCube->SetFontHeight(12);
    myViewCube->SetAxesLabels("", "", "");
    myViewCube->SetTransformPersistence(new Graphic3d_TransformPers(Graphic3d_TMF_TriedronPers,
                                                                    Aspect_TOTP_RIGHT_UPPER,
                                                                    Graphic3d_Vec2i(85, 85)));
    myViewCube->SetViewAnimation(ViewAnimation());
    myViewCube->SetFixedAnimationLoop(false);
    myViewModel->getContext()->Display(myViewCube, false);
}

void OcctView::setupGrid()
{
    // 配置网格
    myViewModel->getContext()->CurrentViewer()->ActivateGrid(Aspect_GT_Rectangular,
                                                             Aspect_GDM_Lines);
}

void OcctView::updateVisibility()
{
    // 使用ViewModel获取全局设置
    auto& globalSettings = myViewModel->getGlobalSettings();

    // 更新网格可见性
    bool isGridVisible = globalSettings.isGridVisible.get();
    if (isGridVisible) {
        myViewModel->getContext()->CurrentViewer()->ActivateGrid(Aspect_GT_Rectangular,
                                                                 Aspect_GDM_Lines);
    }
    else {
        myViewModel->getContext()->CurrentViewer()->DeactivateGrid();
    }

    // 更新视图立方体可见性
    bool isViewCubeVisible = globalSettings.isViewCubeVisible.get();
    if (!myViewCube.IsNull()) {
        if (isViewCubeVisible) {
            myViewModel->getContext()->Display(myViewCube, false);
        }
        else {
            myViewModel->getContext()->Erase(myViewCube, false);
        }
    }

    // 更新显示模式
    int displayMode = globalSettings.displayMode.get();
    int oldDisplayMode = myViewModel->getContext()->DisplayMode();
    if (displayMode != oldDisplayMode) {
        getOcctViewLogger()->info("Updating display mode to {}", displayMode);
        myViewModel->getContext()->SetDisplayMode(displayMode, Standard_True);
    }
    myViewModel->getContext()->UpdateCurrentViewer();

    // 强制重绘视图
    if (!myView.IsNull()) {
        myView->Invalidate();
    }
}

void OcctView::handleSelection(int x, int y)
{
    auto logger = getOcctViewLogger();
    logger->info("Handling selection at position ({}, {})", x, y);

    // Move to the position to detect what's under the cursor
    myViewModel->getContext()->MoveTo(x, y, myView, Standard_True);

    // Check if we're in face selection mode
    if (MVVM::SelectionManager::getInstance().getSelectionMode() == 4) { // Face selection
        // Try to detect a face under cursor
        if (myViewModel->getContext()->HasDetected()) {
            // Get the detected shape (could be a face)
            TopoDS_Shape detectedShape = myViewModel->getContext()->DetectedShape();

            if (!detectedShape.IsNull() && detectedShape.ShapeType() == TopAbs_FACE) {
                logger->info("Detected face under cursor");

                // If we have feature recognition viewmodel, find which feature contains this face
                if (myFeatureRecognitionViewModel) {
                    auto model = myFeatureRecognitionViewModel->getFeatureModel();
                    if (model) {
                        // Get the main shape from the context (the shape that was recognized)
                        Handle(AIS_Shape) mainShape;
                        AIS_ListOfInteractive displayed;
                        myViewModel->getContext()->DisplayedObjects(displayed);

                        for (AIS_ListOfInteractive::Iterator it(displayed); it.More(); it.Next()) {
                            Handle(AIS_Shape) shape = Handle(AIS_Shape)::DownCast(it.Value());
                            if (!shape.IsNull()) {
                                mainShape = shape;
                                break;
                            }
                        }

                        if (!mainShape.IsNull()) {
                            TopoDS_Face detectedFace = TopoDS::Face(detectedShape);
                            std::string faceIdStr    = model->getFaceId(detectedFace);

                            if (!faceIdStr.empty()) {
                                logger->info("Found face ID: {}", faceIdStr);
                                auto locations =
                                    myFeatureRecognitionViewModel->findFeatureLocationsForFace(faceIdStr);
                                if (!locations.empty()) {
                                    const auto& loc = locations.front();
                                    myFeatureRecognitionViewModel->selectFeature(
                                        loc.groupIndex,
                                        loc.subGroupIndex,
                                        loc.featureIndex);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Perform selection and gather results
    myViewModel->getContext()->Select(Standard_True);

    auto buildObjectId = [&](const Handle(AIS_InteractiveObject)& obj) -> std::string {
        std::string id = myViewModel->getObjectId(obj);
        if (!id.empty()) {
            return id;
        }
        std::ostringstream oss;
        oss << "object_" << reinterpret_cast<std::uintptr_t>(obj.get());
        return oss.str();
    };

    auto computeFaceIndex = [](const Handle(AIS_InteractiveObject)& obj,
                               const TopoDS_Shape& selectedShape) -> int {
        Handle(AIS_Shape) aisShape = Handle(AIS_Shape)::DownCast(obj);
        if (aisShape.IsNull()) {
            return -1;
        }

        const TopoDS_Shape& parent = aisShape->Shape();
        if (parent.IsNull()) {
            return -1;
        }

        int index = 1;
        for (TopExp_Explorer exp(parent, TopAbs_FACE); exp.More(); exp.Next(), ++index) {
            if (selectedShape.IsSame(exp.Current())) {
                return index;
            }
        }
        return -1;
    };

    std::vector<MVVM::SelectionInfo::SelectedObject> selectedEntries;
    std::map<std::string, std::vector<MVVM::SelectionInfo::SubFeatureIdentifier>> subFeatureMap;

    auto appendSelectedObject = [&](const std::string& objectId,
                                    const Handle(AIS_InteractiveObject)& obj) {
        auto exists = std::find_if(selectedEntries.begin(),
                                   selectedEntries.end(),
                                   [&](const MVVM::SelectionInfo::SelectedObject& entry) {
                                       return entry.object == obj;
                                   });
        if (exists == selectedEntries.end()) {
            selectedEntries.emplace_back(objectId, obj);
        }
    };

    for (myViewModel->getContext()->InitSelected(); myViewModel->getContext()->MoreSelected();
         myViewModel->getContext()->NextSelected()) {
        Handle(AIS_InteractiveObject) interactive =
            myViewModel->getContext()->SelectedInteractive();
        if (interactive.IsNull()) {
            continue;
        }

        std::string objectId = buildObjectId(interactive);
        appendSelectedObject(objectId, interactive);

        TopoDS_Shape selectedShape = myViewModel->getContext()->SelectedShape();
        if (selectedShape.IsNull()) {
            continue;
        }

        if (selectedShape.ShapeType() == TopAbs_FACE) {
            int faceIndex = computeFaceIndex(interactive, selectedShape);
            MVVM::SelectionInfo::SubFeatureIdentifier identifier(
                MVVM::SelectionInfo::SubFeatureType::Face,
                faceIndex);

            MVVM::SelectionInfo::FaceSelectionData faceData;
            faceData.face = TopoDS::Face(selectedShape);
            if (myFeatureRecognitionViewModel && myFeatureRecognitionViewModel->getFeatureModel()) {
                faceData.id =
                    myFeatureRecognitionViewModel->getFeatureModel()->getFaceId(faceData.face);
            }
            if (faceData.id.empty() && faceIndex > 0) {
                faceData.id = std::to_string(faceIndex);
            }

            identifier.additionalData = faceData;
            subFeatureMap[objectId].push_back(std::move(identifier));
        }
    }

    auto& selectionManager = MVVM::SelectionManager::getInstance();
    if (selectedEntries.empty()) {
        selectionManager.clearSelection();
        logger->info("Selection cleared");
    }
    else {
        logger->info("Selected {} objects", selectedEntries.size());
        selectionManager.setSelection(selectedEntries,
                                      subFeatureMap,
                                      MVVM::SelectionInfo::SelectionType::New);
    }
}

void OcctView::subscribeToEvents()
{
    getOcctViewLogger()->info("Subscribing to events");

    // 使用单个订阅对象订阅多个消息类型
    mySubscriptions = MVVM::MessageBus::getInstance().subscribeMultiple(
        {MVVM::MessageBus::MessageType::ModelChanged,
         MVVM::MessageBus::MessageType::SelectionChanged,
         MVVM::MessageBus::MessageType::ViewChanged},  // 添加 ViewChanged 类型
        [this](const MVVM::MessageBus::Message& message) {
            switch (message.type) {
                case MVVM::MessageBus::MessageType::ModelChanged:
                    // Force view redraw on model change
                    if (!myView.IsNull()) {
                        myView->Invalidate();
                    }
                    break;

                case MVVM::MessageBus::MessageType::SelectionChanged:
                    // Handle selection change
                    try {
                        const auto& selectionInfo =
                            std::any_cast<MVVM::SelectionInfo>(message.data);
                        getOcctViewLogger()->info("Selection changed: {} objects selected",
                                                  selectionInfo.selectedObjects.size());

                        if (myFeatureRecognitionViewModel
                            && myFeatureRecognitionViewModel->getFeatureModel()) {
                            auto featureModel = myFeatureRecognitionViewModel->getFeatureModel();
                            auto faceIds =
                                MVVM::SelectionManager::getInstance().getSelectedFaceIds();

                            bool featureSelected = false;
                            for (const auto& faceId : faceIds) {
                                if (faceId.empty()) {
                                    continue;
                                }

                                auto locations =
                                    myFeatureRecognitionViewModel->findFeatureLocationsForFace(faceId);
                                if (!locations.empty()) {
                                    const auto& loc = locations.front();
                                    if (myFeatureRecognitionViewModel->selectedGroupIndex.get() != loc.groupIndex
                                        || myFeatureRecognitionViewModel->selectedSubGroupIndex.get() != loc.subGroupIndex
                                        || myFeatureRecognitionViewModel->selectedFeatureIndex.get() != loc.featureIndex) {
                                        myFeatureRecognitionViewModel->selectFeature(
                                            loc.groupIndex,
                                            loc.subGroupIndex,
                                            loc.featureIndex);
                                    }
                                    featureSelected = true;
                                    break;
                                }
                            }

                            if (!featureSelected
                                && (myFeatureRecognitionViewModel->selectedGroupIndex.get() != -1
                                    || myFeatureRecognitionViewModel->selectedSubGroupIndex.get() != -1
                                    || myFeatureRecognitionViewModel->selectedFeatureIndex.get() != -1)) {
                                myFeatureRecognitionViewModel->clearSelection();
                            }
                        }

                        // Highlight selected objects in the view
                        if (!myView.IsNull()) {
                            myView->Invalidate();
                        }
                    }
                    catch (const std::bad_any_cast& e) {
                        getOcctViewLogger()->error("Failed to cast selection info: {}", e.what());
                    }
                    break;

                case MVVM::MessageBus::MessageType::ViewChanged:
                    // 处理视图变更消息
                    try {
                        const auto& msgData = std::any_cast<std::string>(message.data);
                        if (msgData == "ImGuiContextMenuClosed") {
                            // 菜单关闭后，重置鼠标状态
                            myResetViewInput = true;
                        }
                    }
                    catch (const std::bad_any_cast& e) {
                        getOcctViewLogger()->error("Failed to cast view change data: {}", e.what());
                    }
                    break;
            }
        });

    // Get global settings
    auto& globalSettings = myViewModel->getGlobalSettings();

    // Connect to grid visibility property
    auto gridConn = globalSettings.isGridVisible.valueChanged.connect(
        [this](const bool&, const bool& isVisible) {
            updateVisibility();
        });
    myConnections.track(gridConn);

    // Connect to view cube visibility property
    auto cubeConn = globalSettings.isViewCubeVisible.valueChanged.connect(
        [this](const bool&, const bool& isVisible) {
            updateVisibility();
        });
    myConnections.track(cubeConn);

    // Connect to display mode property
    auto displayConn =
        globalSettings.displayMode.valueChanged.connect([this](const int&, const int& mode) {
            updateVisibility();
        });
    myConnections.track(displayConn);

    // 不再需要连接到选择属性，因为现在使用 SelectionManager
    // 订阅 SelectionChanged 消息已经足够
}

// IView 接口实现
void OcctView::initialize(GLFWwindow* window)
{
    getOcctViewLogger()->info("OcctView: Initializing with GLFW window");
    // 调用原始的初始化方法
    initialize();
}

void OcctView::newFrame()
{
    // OcctView 不需要为每一帧做特殊准备
    // 这个方法是为了满足 IView 接口
}

void OcctView::shutdown()
{
    getOcctViewLogger()->info("OcctView: Shutting down");
    cleanup();
}

bool OcctView::wantCaptureMouse() const
{
    // OcctView 通常需要捕获鼠标事件，但不应该阻止其他视图
    // 返回 false 允许事件继续传播
    return false;
}

std::shared_ptr<IViewModel> OcctView::getViewModel() const
{
    return std::static_pointer_cast<IViewModel>(myViewModel);
}

void OcctView::setFeatureRecognitionViewModel(std::shared_ptr<FeatureRecognitionViewModel> viewModel)
{
    getOcctViewLogger()->info("Setting feature recognition viewmodel");
    myFeatureRecognitionViewModel = viewModel;

    // Subscribe to feature selection events
    if (myFeatureRecognitionViewModel) {
        myConnections.track(
            myFeatureRecognitionViewModel->onFeatureSelected.connect(
                [this](int groupIdx, int subGroupIdx, int featureIdx) {
                    getOcctViewLogger()->debug("Feature selected: group={}, subGroup={}, feature={}",
                                                 groupIdx, subGroupIdx, featureIdx);
                    refreshCurrentFeatureHighlight();
                }));

        myConnections.track(
            myFeatureRecognitionViewModel->onRecognitionCompleted.connect([this]() {
                updateFeatureOverview();
            }));

        myConnections.track(
            myFeatureRecognitionViewModel->onFeatureVisibilityChanged.connect(
                [this](int groupIdx, bool visible) {
                    getOcctViewLogger()->debug("Feature group {} visibility changed to {}",
                                               groupIdx, visible);
                    updateFeatureOverview();
                    refreshCurrentFeatureHighlight();
                }));

        // Subscribe to results availability (covers manual JSON load as well)
        myConnections.track(
            myFeatureRecognitionViewModel->hasResults.valueChanged.connect(
                [this](const bool&, const bool& hasResults) {
                    if (hasResults) {
                        updateFeatureOverview();
                    }
                    else {
                        clearFeatureHighlights();
                        clearFeatureOverview();
                    }
                }));

        if (myFeatureRecognitionViewModel->hasResults.get()) {
            updateFeatureOverview();
        }
        else {
            clearFeatureOverview();
        }
    }
    else {
        clearFeatureHighlights();
        clearFeatureOverview();
    }
}

void OcctView::highlightFeatureFaces(const std::vector<std::string>& faceIDs,
                                     const Quantity_Color& color)
{
    auto logger = getOcctViewLogger();
    logger->debug("Highlighting {} faces", faceIDs.size());

    if (!myFeatureRecognitionViewModel || !myFeatureRecognitionViewModel->getFeatureModel()) {
        logger->warn("No feature recognition model available");
        return;
    }

    auto featureModel = myFeatureRecognitionViewModel->getFeatureModel();
    auto context = myViewModel->getContext();

    // Clear previous highlights
    if (!myFeatureHighlightShape.IsNull()) {
        context->Remove(myFeatureHighlightShape, false);
        myFeatureHighlightShape.Nullify();
    }

    if (faceIDs.empty()) {
        logger->debug("No faces supplied for highlighting");
        if (!myView.IsNull()) {
            myView->Redraw();
        }
        return;
    }

    BRep_Builder builder;
    TopoDS_Compound highlightCompound;
    builder.MakeCompound(highlightCompound);

    Standard_Boolean hasFaces = Standard_False;
    for (const auto& faceID : faceIDs) {
        TopoDS_Face face = featureModel->getFaceByID(faceID);
        if (!face.IsNull()) {
            builder.Add(highlightCompound, face);
            hasFaces = Standard_True;
            logger->trace("Highlight face {}", faceID);
        } else {
            logger->warn("Face with ID {} not found", faceID);
        }
    }

    if (!hasFaces) {
        logger->warn("No valid faces found to highlight");
        if (!myView.IsNull()) {
            myView->Redraw();
        }
        return;
    }

    Handle(AIS_ColoredShape) highlightShape = new AIS_ColoredShape(highlightCompound);
    highlightShape->SetDisplayMode(AIS_Shaded);
    highlightShape->SetMaterial(Graphic3d_NOM_PLASTIC);

    // 使用固定的高亮黄颜色，使选中特征在任何场景下都足够醒目
    Quantity_Color highlightColor(1.0, 0.9, 0.05, Quantity_TOC_RGB);
    highlightShape->SetColor(highlightColor);
    highlightShape->SetTransparency(0.02f);
    highlightShape->Attributes()->SetFaceBoundaryDraw(true);
    highlightShape->Attributes()->SetFaceBoundaryAspect(
        new Prs3d_LineAspect(Quantity_Color(0.1, 0.1, 0.1, Quantity_TOC_RGB),
                             Aspect_TOL_SOLID,
                             3.0f));

    myFeatureHighlightShape = highlightShape;
    context->Display(myFeatureHighlightShape, AIS_Shaded, 0, false);
    context->Deactivate(myFeatureHighlightShape);

    if (!myView.IsNull()) {
        myView->Redraw();
    }

    logger->info("Highlighted {} faces with color RGB({:.2f}, {:.2f}, {:.2f})",
                 faceIDs.size(), color.Red(), color.Green(), color.Blue());
}

void OcctView::refreshCurrentFeatureHighlight()
{
    if (!myFeatureRecognitionViewModel) {
        clearFeatureHighlights();
        return;
    }

    const int groupIdx = myFeatureRecognitionViewModel->selectedGroupIndex.get();
    if (groupIdx < 0) {
        clearFeatureHighlights();
        return;
    }

    const int subGroupIdx = myFeatureRecognitionViewModel->selectedSubGroupIndex.get();
    const int featureIdx = myFeatureRecognitionViewModel->selectedFeatureIndex.get();
    auto faceIDs = myFeatureRecognitionViewModel->getFeatureFaceIDs(groupIdx, subGroupIdx, featureIdx);

    if (faceIDs.empty()) {
        clearFeatureHighlights();
        return;
    }

    auto color = myFeatureRecognitionViewModel->getFeatureGroupColor(groupIdx);
    highlightFeatureFaces(faceIDs, color);
}

void OcctView::clearFeatureHighlights()
{
    auto logger = getOcctViewLogger();
    logger->debug("Clearing feature highlights");

    if (!myFeatureHighlightShape.IsNull() && myViewModel) {
        auto context = myViewModel->getContext();
        if (!context.IsNull()) {
            context->Remove(myFeatureHighlightShape, false);
        }
        myFeatureHighlightShape.Nullify();
        logger->info("Feature highlights cleared");
        if (!myView.IsNull()) {
            myView->Redraw();
        }
    }
}

void OcctView::updateFeatureOverview()
{
    auto logger = getOcctViewLogger();

    if (!myFeatureRecognitionViewModel || !myViewModel) {
        clearFeatureOverview();
        return;
    }

    auto featureModel = myFeatureRecognitionViewModel->getFeatureModel();
    if (!featureModel || !featureModel->hasResults()) {
        clearFeatureOverview();
        return;
    }

    auto context = myViewModel->getContext();
    if (context.IsNull()) {
        logger->warn("Interactive context is null, cannot update feature overview");
        return;
    }

    const TopoDS_Shape& originalShape = featureModel->getOriginalShape();
    if (originalShape.IsNull()) {
        logger->warn("Original shape is null, cannot build feature overview");
        clearFeatureOverview();
        return;
    }

    Handle(AIS_ColoredShape) overview = new AIS_ColoredShape(originalShape);
    overview->SetDisplayMode(AIS_Shaded);
    overview->SetMaterial(Graphic3d_NOM_PLASTIC);
    overview->SetTransparency(0.65f);
    overview->SetColor(Quantity_Color(0.32, 0.36, 0.42, Quantity_TOC_RGB));
    overview->Attributes()->SetFaceBoundaryDraw(true);
    overview->Attributes()->SetFaceBoundaryAspect(
        new Prs3d_LineAspect(Quantity_Color(0.25, 0.25, 0.25, Quantity_TOC_RGB),
                             Aspect_TOL_SOLID,
                             1.0f));

    const auto& groups = featureModel->getFeatureGroups();
    for (int groupIdx = 0; groupIdx < static_cast<int>(groups.size()); ++groupIdx) {
        const auto& group = groups[groupIdx];
        if (!featureModel->isGroupVisible(groupIdx)) {
            continue;
        }
        auto faceIDs = featureModel->getFaceIDsForFeature(groupIdx, -1, -1);
        if (faceIDs.empty()) {
            continue;
        }

        for (const auto& faceID : faceIDs) {
            TopoDS_Face face = featureModel->getFaceByID(faceID);
            if (face.IsNull()) {
                continue;
            }

            overview->SetCustomColor(face, group.color);
            overview->SetCustomTransparency(face, 0.1f);
        }
    }

    if (!myFeatureOverviewShape.IsNull()) {
        context->Remove(myFeatureOverviewShape, false);
    }

    myFeatureOverviewShape = overview;
    context->Display(myFeatureOverviewShape, AIS_Shaded, 0, false);
    context->Deactivate(myFeatureOverviewShape);

    if (!myFeatureHighlightShape.IsNull()) {
        context->Display(myFeatureHighlightShape, AIS_Shaded, 0, false);
        context->Deactivate(myFeatureHighlightShape);
    }

    if (!myView.IsNull()) {
        myView->Redraw();
    }

    logger->info("Updated feature overview overlay for {} groups", groups.size());
}

void OcctView::clearFeatureOverview()
{
    if (!myViewModel) {
        myFeatureOverviewShape.Nullify();
        return;
    }

    if (myFeatureOverviewShape.IsNull()) {
        return;
    }

    auto context = myViewModel->getContext();
    if (!context.IsNull()) {
        context->Remove(myFeatureOverviewShape, false);
    }

    myFeatureOverviewShape.Nullify();

    if (!myView.IsNull()) {
        myView->Redraw();
    }
}

