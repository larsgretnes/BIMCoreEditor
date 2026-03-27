// =============================================================================
// BimCore/apps/editor/ui/UIPropertiesPanel.h
// =============================================================================
#pragma once
#include "ui/UIState.h"
#include "scene/SceneModel.h"
#include "core/CommandHistory.h"
#include <vector>
#include <memory>
#include <string>
#include <imgui.h>

namespace BimCore {
    class UIPropertiesPanel {
    public:
        static void Render(SelectionState& state, std::vector<std::shared_ptr<SceneModel>>& documents, bool& triggerFocus, CommandHistory& history);
        
    private:
        static std::shared_ptr<SceneModel> FindOwnerDocument(const std::string& guid, const std::vector<std::shared_ptr<SceneModel>>& documents);
        static void DrawSharedPropertyTable(SelectionState& state, std::vector<std::shared_ptr<SceneModel>>& documents, const std::string& locFilter, const ImVec2& sqBtn, bool& globalRefreshNeeded, CommandHistory& history);
        static void DrawPropertyTable(SelectionState& state, SelectedObject& obj, std::vector<std::shared_ptr<SceneModel>>& documents, const std::string& locFilter, const ImVec2& sqBtn, bool& objNeedsRefresh, std::string& propToDelete, CommandHistory& history);
    };
}