// =============================================================================
// BimCore/apps/editor/ui/UIMainPanel.h
// =============================================================================
#pragma once
#include "UIState.h"
#include "scene/SceneModel.h"
#include "graphics/Camera.h" // --- FIXED: Give the header access to the Camera ---
#include <vector>
#include <memory>

namespace BimCore {
    class UIMainPanel {
    public:
        // --- FIXED: Added Camera* camera to the signature ---
        static void Render(SelectionState& state, std::vector<std::shared_ptr<SceneModel>>& documents, float configMaxExplode, bool& triggerFocus, bool& triggerRebuild, Camera* camera);
        
    private:
        static void HandleShiftSelection(SelectionState& state, int visualIdx, uint32_t meshIdx, const std::string& groupName, const std::vector<uint32_t>& currentArray, std::shared_ptr<SceneModel> document);
        static void DrawResetModal(SelectionState& state, std::vector<std::shared_ptr<SceneModel>>& documents, bool& triggerRebuild);
    };
} // namespace BimCore