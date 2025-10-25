#pragma once

#include "mvvm/MessageBus.h"
#include <AIS_InteractiveObject.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace MVVM
{

class SelectionManager
{
public:
    // 获取单例实例
    static SelectionManager& getInstance();

    // 删除拷贝构造函数和赋值操作符
    SelectionManager(const SelectionManager&) = delete;
    SelectionManager& operator=(const SelectionManager&) = delete;

    // Selection methods
    void addToSelection(const Handle(AIS_InteractiveObject) & object, const std::string& objectId);
    void addToSelection(const Handle(AIS_InteractiveObject) & object,
                        const std::string& objectId,
                        const std::vector<SelectionInfo::SubFeatureIdentifier>& subFeatures);

    void removeFromSelection(const Handle(AIS_InteractiveObject) & object,
                             const std::string& objectId);
    void removeFromSelection(const std::string& objectId);

    void clearSelection();

    // Set selection mode
    void setSelectionMode(int mode);

    // Set selection type
    void setSelectionType(SelectionInfo::SelectionType type);

    // Get current selection
    const SelectionInfo& getCurrentSelection() const;

    // Check if there is any selection
    bool hasSelection() const;

    // Get the primary selected shape (first AIS_Shape)
    TopoDS_Shape getSelectedShape() const;

    // 获取当前所有选中的面（仅当 selectionMode 支持时）
    std::vector<TopoDS_Face> getSelectedFaces() const;

    // 获取当前所有选中面的 ID（若有）
    std::vector<std::string> getSelectedFaceIds() const;

    // Get current selection mode
    int getSelectionMode() const;

    // Replace entire selection
    void setSelection(const std::vector<SelectionInfo::SelectedObject>& objects,
                      const std::map<std::string, std::vector<SelectionInfo::SubFeatureIdentifier>>& subFeatures,
                      SelectionInfo::SelectionType type = SelectionInfo::SelectionType::New);

private:
    // 私有构造函数
    SelectionManager();

    // Notify selection changes through MessageBus
    void notifySelectionChanged();

    // MessageBus reference
    MessageBus& myMessageBus;

    // Current selection state
    SelectionInfo mySelectionInfo;
};

}  // namespace MVVM
