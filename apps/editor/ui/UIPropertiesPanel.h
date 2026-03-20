// =============================================================================
// BimCore/apps/editor/ui/UIPropertiesPanel.h
// =============================================================================
#pragma once
#include "UIState.h"
#include <memory>

namespace BimCore {
    class UIPropertiesPanel {
    public:
        void Render(SelectionState& state, std::shared_ptr<BimDocument> document, bool& triggerFocus);
    private:
        void DrawSharedPropertyTable(SelectionState& state, std::shared_ptr<BimDocument> document, const std::string& locFilter, const ImVec2& sqBtn, bool& globalRefreshNeeded);
        void DrawPropertyTable(SelectionState& state, SelectedObject& obj, std::shared_ptr<BimDocument> document, const std::string& locFilter, const ImVec2& sqBtn, bool& objNeedsRefresh, std::string& propToDelete);
    };
}
