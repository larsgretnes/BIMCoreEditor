// =============================================================================
// BimCore/apps/editor/ui/UISearchPanel.h
// =============================================================================
#pragma once
#include "ui/UIState.h"
#include "scene/SceneModel.h"
#include <vector>
#include <memory>
#include <string>

namespace BimCore {
    class UISearchPanel {
    public:
        static void Render(SelectionState& state, std::vector<std::shared_ptr<SceneModel>>& documents, bool& triggerFocus);
        static void ExecuteTextSearch(const std::string& query, std::vector<std::shared_ptr<SceneModel>>& documents, SelectionState& state);
    };
} // namespace BimCore