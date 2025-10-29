#include "GeometryViewModel.h"
#include "ais/Mesh_DataSource.h"
#include "utils/Logger.h"
#include <AIS_Shape.hxx>
#include <AIS_Triangulation.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <MeshVS_DisplayModeFlags.hxx>
#include <MeshVS_Drawer.hxx>
#include <MeshVS_DrawerAttribute.hxx>
#include <MeshVS_Mesh.hxx>
#include <MeshVS_MeshPrsBuilder.hxx>
#include <Precision.hxx>
#include <TColStd_HPackedMapOfInteger.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS_Builder.hxx>
#include <algorithm>
#include <iostream>
#include <random>


// 使用宏声明 GeometryViewModel 类的 logger
DECLARE_LOGGER(GeometryViewModel)

// Constructor
GeometryViewModel::GeometryViewModel(std::shared_ptr<GeometryModel> model,
                                     Handle(AIS_InteractiveContext) context,
                                     MVVM::GlobalSettings& globalSettings,
                                     std::shared_ptr<ModelImporter> modelImporter)
    : myModel(model)
    , myContext(context)
    , myGlobalSettings(globalSettings)
    , myModelImporter(modelImporter)
{

    // Register model change listener
    model->addChangeListener([this](const std::string& id) {
        this->onModelChanged(id);
    });

    // Initialize display of existing geometries
    for (const std::string& id : model->getAllEntityIds()) {
        updatePresentation(id);
    }
}

// Command - CAD geometry operations
void GeometryViewModel::createBox(const gp_Pnt& location, double sizeX, double sizeY, double sizeZ)
{
    // Generate unique ID
    static int boxCounter = 0;
    std::string id = "box_" + std::to_string(++boxCounter);

    // Create box geometry
    BRepPrimAPI_MakeBox boxMaker(location, sizeX, sizeY, sizeZ);
    TopoDS_Shape boxShape = boxMaker.Shape();

    // Add to model
    myModel->addShape(id, boxShape);
}

void GeometryViewModel::createCone(const gp_Pnt& location, double radius, double height)
{
    // Generate unique ID
    static int coneCounter = 0;
    std::string id = "cone_" + std::to_string(++coneCounter);

    // Create cone geometry
    gp_Ax2 axis(location, gp_Dir(0, 0, 1));
    BRepPrimAPI_MakeCone coneMaker(axis, radius, 0, height);
    TopoDS_Shape coneShape = coneMaker.Shape();

    // Add to model
    myModel->addShape(id, coneShape);
}

void GeometryViewModel::createMesh(/* Mesh creation parameters */)
{
    // This method needs to be implemented based on the actual mesh creation requirements
    // Below is example code, should be modified based on actual situation

    /*
    // Generate unique ID
    static int meshCounter = 0;
    std::string id = "mesh_" + std::to_string(++meshCounter);

    // Create triangle mesh (example)
    Handle(Poly_Triangulation) mesh = new Poly_Triangulation(numVertices, numTriangles, false);

    // Set vertices and triangles
    // ... fill mesh data ...

    // Add to model
    myModel->addMesh(id, mesh);
    */
}

bool GeometryViewModel::importModel(const std::string& filePath, const std::string& modelId)
{
    LOG_FUNCTION_SCOPE(getGeometryViewModelLogger(), "importModel");
    getGeometryViewModelLogger()->info("Importing model from '{}'", filePath);

    if (!myModelImporter) {
        getGeometryViewModelLogger()->error("ModelImporter is not available");
        return false;
    }

    // 使用注入的 ModelImporter 导入模型
    bool result = myModelImporter->importModel(filePath, *myModel, modelId);

    if (result) {
        getGeometryViewModelLogger()->info("Model imported successfully");
    }
    else {
        getGeometryViewModelLogger()->error("Failed to import model");
    }

    return result;
}

// IViewModel interface implementation
void GeometryViewModel::deleteSelectedObjects()
{
    // Get selected objects from SelectionManager
    const auto& selectionInfo = MVVM::SelectionManager::getInstance().getCurrentSelection();
    std::vector<std::string> objectsToDelete;

    // Extract object IDs from the selection
    for (const auto& entry : selectionInfo.selectedObjects) {
        std::string id = !entry.id.empty() ? entry.id : getObjectId(entry.object);
        if (!id.empty()) {
            objectsToDelete.push_back(id);
        }
    }

    // Delete the objects
    for (const std::string& id : objectsToDelete) {
        myModel->removeEntity(id);
    }

    // Clear the selection
    MVVM::SelectionManager::getInstance().clearSelection();
}

// Attribute access and modification
void GeometryViewModel::setSelectedColor(const Quantity_Color& color)
{
    // Get selected objects from SelectionManager
    auto selectedObjects =
        MVVM::SelectionManager::getInstance().getCurrentSelection().selectedObjects;

    for (const auto& entry : selectedObjects) {
        std::string id = !entry.id.empty() ? entry.id : getObjectId(entry.object);
        if (!id.empty()) {
            myModel->setColor(id, color);
        }
    }
}

Quantity_Color GeometryViewModel::getSelectedColor() const
{
    auto selectedObjects =
        MVVM::SelectionManager::getInstance().getCurrentSelection().selectedObjects;

    if (selectedObjects.empty()) {
        return Quantity_Color(0.8, 0.8, 0.8, Quantity_TOC_RGB);  // Default gray
    }

    // Return color of the first selected object
    const auto& first = selectedObjects.front();
    std::string id    = !first.id.empty() ? first.id : getObjectId(first.object);
    if (!id.empty()) {
        return myModel->getColor(id);
    }

    return Quantity_Color(0.8, 0.8, 0.8, Quantity_TOC_RGB);
}

std::string GeometryViewModel::getObjectId(const Handle(AIS_InteractiveObject)& object) const
{
    auto it = myObjectToIdMap.find(object);
    if (it != myObjectToIdMap.end()) {
        return it->second;
    }
    return {};
}

// Private methods
void GeometryViewModel::updatePresentation(const std::string& id)
{
    // Delete existing representation
    auto it = myIdToObjectMap.find(id);
    if (it != myIdToObjectMap.end()) {
        myContext->Remove(it->second, false);
        myObjectToIdMap.erase(it->second);
        myIdToObjectMap.erase(it);
    }

    // Get geometry data
    const GeometryModel::GeometryData* data = myModel->getGeometryData(id);
    if (!data) {
        myVisibilityStates.erase(id);
        MVVM::SelectionManager::getInstance().removeFromSelection(id);
        return;
    }

    // Create new representation
    Handle(AIS_InteractiveObject) aisObj = createPresentationForGeometry(id, data);
    if (aisObj.IsNull()) {
        myVisibilityStates.erase(id);
        MVVM::SelectionManager::getInstance().removeFromSelection(id);
        return;
    }

    // Update mapping
    myIdToObjectMap[id] = aisObj;
    myObjectToIdMap[aisObj] = id;

    bool visible = true;
    auto visibilityIt = myVisibilityStates.find(id);
    if (visibilityIt != myVisibilityStates.end()) {
        visible = visibilityIt->second;
    }
    myVisibilityStates[id] = visible;

    // Display object (even if hidden, register with context then erase)
    myContext->Display(aisObj, false);
    if (!visible) {
        myContext->Erase(aisObj, false);
        MVVM::SelectionManager::getInstance().removeFromSelection(id);
    }

    // Activate face selection for AIS_Shape objects to support feature picking.
    Handle(AIS_Shape) displayedShape = Handle(AIS_Shape)::DownCast(aisObj);
    if (!displayedShape.IsNull()) {
        myContext->Activate(displayedShape, AIS_Shape::SelectionMode(TopAbs_FACE), Standard_False);
    }
}

Handle(AIS_InteractiveObject)
    GeometryViewModel::createPresentationForGeometry(const std::string& id,
                                                     const GeometryModel::GeometryData* data)
{

    if (!data) {
        return nullptr;
    }

    Handle(AIS_InteractiveObject) aisObj;

    // Create appropriate AIS object based on geometry type
    if (data->type == GeometryModel::GeometryType::SHAPE) {
        // Create AIS_Shape for CAD shape
        const TopoDS_Shape& shape = std::get<TopoDS_Shape>(data->geometry);
        Handle(AIS_Shape) aisShape = new AIS_Shape(shape);
        aisShape->SetColor(data->color);

        // Set display mode based on displayMode
        switch (myGlobalSettings.displayMode.get()) {
            case 0:  // Shaded
                aisShape->SetDisplayMode(AIS_Shaded);
                break;
            case 1:  // Wireframe
                aisShape->SetDisplayMode(AIS_WireFrame);
                break;
                // Can add more display modes
        }

        aisObj = aisShape;
    }
    else if (data->type == GeometryModel::GeometryType::MESH) {
        // Get the mesh data from the geometry
        const GeometryModel::MeshData& meshData = std::get<GeometryModel::MeshData>(data->geometry);
        Handle(Mesh_DataSource) meshDataSource =
            new Mesh_DataSource(meshData.vertices, meshData.faces, meshData.normals);

        Handle(MeshVS_Mesh) meshObj = new MeshVS_Mesh;
        meshObj->SetDataSource(meshDataSource);

        Handle(MeshVS_MeshPrsBuilder) mainBuilder =
            new MeshVS_MeshPrsBuilder(meshObj, MeshVS_DMF_WireFrame | MeshVS_DMF_Shading);
        meshObj->AddBuilder(mainBuilder, true);
        meshObj->GetDrawer()->SetColor(MeshVS_DA_EdgeColor, data->color);

        // Hide all nodes by default
        Handle(TColStd_HPackedMapOfInteger) aNodes =
            new TColStd_HPackedMapOfInteger(meshDataSource->GetAllNodes());
        meshObj->SetHiddenNodes(aNodes);

        meshObj->SetDisplayMode(MeshVS_DMF_Shading);

        aisObj = meshObj;
    }

    return aisObj;
}

void GeometryViewModel::onModelChanged(const std::string& id)
{
    updatePresentation(id);
}

bool GeometryViewModel::isObjectVisible(const std::string& id) const
{
    auto it = myVisibilityStates.find(id);
    if (it != myVisibilityStates.end()) {
        return it->second;
    }
    return true;
}

void GeometryViewModel::setObjectVisibility(const std::string& id, bool visible)
{
    myVisibilityStates[id] = visible;

    auto it = myIdToObjectMap.find(id);
    if (it == myIdToObjectMap.end()) {
        if (!visible) {
            MVVM::SelectionManager::getInstance().removeFromSelection(id);
        }
        return;
    }

    Handle(AIS_InteractiveObject) aisObj = it->second;
    if (aisObj.IsNull()) {
        return;
    }

    if (visible) {
        myContext->Display(aisObj, false);
        Handle(AIS_Shape) aisShape = Handle(AIS_Shape)::DownCast(aisObj);
        if (!aisShape.IsNull()) {
            myContext->Activate(aisShape, AIS_Shape::SelectionMode(TopAbs_FACE), Standard_False);
        }
    }
    else {
        myContext->Erase(aisObj, false);
        myContext->Deactivate(aisObj);
        MVVM::SelectionManager::getInstance().removeFromSelection(id);
    }

    myContext->UpdateCurrentViewer();
}

bool GeometryViewModel::toggleObjectVisibility(const std::string& id)
{
    bool newState = !isObjectVisible(id);
    setObjectVisibility(id, newState);
    return newState;
}

void GeometryViewModel::selectObject(const std::string& id, bool append)
{
    auto it = myIdToObjectMap.find(id);
    if (it == myIdToObjectMap.end()) {
        return;
    }

    Handle(AIS_InteractiveObject) aisObj = it->second;
    if (aisObj.IsNull()) {
        return;
    }

    if (!isObjectVisible(id)) {
        setObjectVisibility(id, true);
    }

    auto& selectionManager = MVVM::SelectionManager::getInstance();
    if (!append) {
        myContext->ClearSelected(false);
    }
    selectionManager.setSelectionType(
        append ? MVVM::SelectionInfo::SelectionType::Add : MVVM::SelectionInfo::SelectionType::New);
    selectionManager.addToSelection(aisObj, id);
    selectionManager.setSelectionType(MVVM::SelectionInfo::SelectionType::New);

    myContext->AddSelect(aisObj);
    myContext->UpdateCurrentViewer();
}

Handle(AIS_InteractiveObject) GeometryViewModel::getPresentation(const std::string& id) const
{
    auto it = myIdToObjectMap.find(id);
    if (it != myIdToObjectMap.end()) {
        return it->second;
    }
    return Handle(AIS_InteractiveObject)();
}
