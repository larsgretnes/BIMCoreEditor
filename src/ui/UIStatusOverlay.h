// =============================================================================
// BimCore/apps/editor/ui/UIStatusOverlay.h
// =============================================================================
#pragma once
#include "ui/UIState.h"
#include "scene/SceneModel.h"
#include "core/CommandHistory.h" // Added this
#include <vector>
#include <memory>

namespace BimCore {
    class UIStatusOverlay {
    public:
        void RenderFlyMode(bool isFlightMode);
        void RenderStatusPanel(SelectionState& state, std::vector<std::shared_ptr<SceneModel>>& documents);
        
        // Updated signature to accept CommandHistory
        void RenderContextMenu(SelectionState& state, bool& triggerFocus, CommandHistory& history); 
    };
}