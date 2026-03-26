// =============================================================================
// BimCore/apps/editor/ui/UIModelTree.h
// =============================================================================
#pragma once
#include "ui/UIState.h"
#include "scene/SceneModel.h"
#include <vector>
#include <memory>
#include <string>

namespace BimCore {
    class UIModelTree {
    public:
        static void Render(SelectionState& state, std::vector<std::shared_ptr<SceneModel>>& documents, bool& triggerFocus, bool& triggerRebuild);
        
        // Exposed for UISearchPanel to use
        static void HandleShiftSelection(SelectionState& state, int visualIdx, uint32_t meshIdx, const std::string& groupName, const std::vector<uint32_t>& currentArray, std::shared_ptr<SceneModel> document);
        static void DrawMultiSelectContextMenu(SelectionState& state, const RenderSubMesh* sub, std::shared_ptr<SceneModel> doc, bool& triggerFocus);
    };
} // namespace BimCore