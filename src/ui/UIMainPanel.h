// =============================================================================
// BimCore/apps/editor/ui/UIMainPanel.h
// =============================================================================
#pragma once
#include "ui/UIState.h"
#include "scene/SceneModel.h"
#include "graphics/Camera.h" 
#include "core/CommandHistory.h"
#include <vector>
#include <memory>

namespace BimCore {
    class UIMainPanel {
    public:
        static void Render(SelectionState& state, std::vector<std::shared_ptr<SceneModel>>& documents, float configMaxExplode, bool& triggerFocus, bool& triggerRebuild, Camera* camera, CommandHistory& history);
        
    private:
        static void HandleShiftSelection(SelectionState& state, int visualIdx, uint32_t meshIdx, const std::string& groupName, const std::vector<uint32_t>& currentArray, std::shared_ptr<SceneModel> document);
        
        // --- FIXED: Pass CommandHistory down to the modal ---
        static void DrawResetModal(SelectionState& state, std::vector<std::shared_ptr<SceneModel>>& documents, bool& triggerRebuild, CommandHistory& history);
    };
} // namespace BimCore