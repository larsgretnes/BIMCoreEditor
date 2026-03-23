// =============================================================================
// BimCore/apps/editor/ui/UIPropertiesPanel.h
// =============================================================================
#pragma once
#include "UIState.h"
#include <memory>
#include <vector>

namespace BimCore {
    class UIPropertiesPanel {
    public:
        void Render(SelectionState& state, std::vector<std::shared_ptr<SceneModel>>& documents, bool& triggerFocus);
    private:
        void DrawSharedPropertyTable(SelectionState& state, std::vector<std::shared_ptr<SceneModel>>& documents, const std::string& locFilter, const ImVec2& sqBtn, bool& globalRefreshNeeded);
        void DrawPropertyTable(SelectionState& state, SelectedObject& obj, std::vector<std::shared_ptr<SceneModel>>& documents, const std::string& locFilter, const ImVec2& sqBtn, bool& objNeedsRefresh, std::string& propToDelete);

        // Helper to find the specific document that owns a GUID
        std::shared_ptr<SceneModel> FindOwnerDocument(const std::string& guid, const std::vector<std::shared_ptr<SceneModel>>& documents);
    };
}
