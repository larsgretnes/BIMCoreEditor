// =============================================================================
// BimCore/apps/editor/ui/AppUI.h
// =============================================================================
#pragma once
#include "ui/UIMainPanel.h"
#include "ui/UIPropertiesPanel.h"
#include "ui/UIStatusOverlay.h"
#include "ui/UICommandPanel.h" // <--- INKLUDERT
#include "ui/UIState.h"
#include "graphics/GraphicsContext.h"
#include "graphics/Camera.h"
#include "scene/SceneModel.h"
#include "core/EngineConfig.h" // <--- INKLUDERT
#include "core/CommandHistory.h" 

struct GLFWwindow; 

namespace BimCore {
    class AppUI {
    public:
        void NewFrame();
        
        // <--- OPPDATERT: Tar nå inn EngineConfig& config
        void Render(SelectionState& selection, GraphicsContext& graphics, std::vector<std::shared_ptr<SceneModel>>& documents, Camera& camera, EngineConfig& config, bool& triggerFocus, bool isFlightMode, bool& triggerRebuild, CommandHistory* history, GLFWwindow* window);

        SelectionState state;

    private:
        UIMainPanel m_mainPanel;
        UIPropertiesPanel m_propertiesPanel;
        UIStatusOverlay m_overlay;
        float m_fpsTracker[90] = {0};
    };
} // namespace BimCore