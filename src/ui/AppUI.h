// =============================================================================
// BimCore/apps/editor/ui/AppUI.h
// =============================================================================
#pragma once
#include "ui/UIMainPanel.h"
#include "ui/UIPropertiesPanel.h"
#include "ui/UIStatusOverlay.h"
#include "ui/UIState.h"
#include "graphics/GraphicsContext.h"
#include "graphics/Camera.h"
#include "scene/SceneModel.h"
#include "core/CommandHistory.h" 

struct GLFWwindow; // <--- NEW: Forward declare GLFWwindow

namespace BimCore {
    class AppUI {
    public:
        void NewFrame();
        
        // <--- FIXED: Added GLFWwindow* window to the end of the signature
        void Render(SelectionState& selection, GraphicsContext& graphics, std::vector<std::shared_ptr<SceneModel>>& documents, Camera& camera, float configMaxExplode, bool& triggerFocus, bool isFlightMode, bool& triggerRebuild, CommandHistory* history, GLFWwindow* window);

        SelectionState state;

    private:
        UIMainPanel m_mainPanel;
        UIPropertiesPanel m_propertiesPanel;
        UIStatusOverlay m_overlay;
        float m_fpsTracker[90] = {0};
    };
} // namespace BimCore