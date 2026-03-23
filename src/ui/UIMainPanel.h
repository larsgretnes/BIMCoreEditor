// =============================================================================
// BimCore/apps/editor/ui/UIMainPanel.h
// =============================================================================
#pragma once
#include "UIState.h"
#include <memory>
#include <vector>

namespace BimCore {
    class UIMainPanel {
    public:
        void Render(SelectionState& state, std::vector<std::shared_ptr<SceneModel>>& documents, float configMaxExplode, bool& triggerFocus, bool& triggerRebuild);
    private:
        void DrawResetModal(SelectionState& state, std::vector<std::shared_ptr<SceneModel>>& documents, bool& triggerRebuild);
        void HandleShiftSelection(SelectionState& state, int visualIdx, uint32_t subMeshIdx, const std::string& groupName, const std::vector<uint32_t>& currentArray, std::shared_ptr<SceneModel> document);
    };
}
