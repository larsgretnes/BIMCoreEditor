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
    private:
        static bool IContains(const std::string& str, const std::string& query);
    };
} // namespace BimCore