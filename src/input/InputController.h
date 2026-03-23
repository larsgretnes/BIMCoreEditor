// =============================================================================
// BimCore/apps/editor/InputController.h
// =============================================================================
#pragma once

#include <memory>
#include <vector>

#include "platform/Window.h"
#include "graphics/Camera.h"
#include "scene/SceneModel.h"
#include "scene/Raycaster.h"
#include "ui/AppUI.h"
#include "core/EngineConfig.h"

namespace BimCore {

    // =============================================================================
    // Enums
    // =============================================================================

    enum class NavigationMode {
        Flight,
        CAD
    };

    // =============================================================================
    // InputController Class
    // =============================================================================

    class InputController {
    public:
        void Update(Window&                      window,
                    Camera&                      camera,
                    std::vector<std::shared_ptr<SceneModel>>& documents,
                    SelectionState&              selection,
                    const EngineConfig&          config,
                    float                        deltaTime,
                    uint32_t&                    currentLightingMode,
                    bool&                        triggerFocus);

        bool IsFlightMode() const;

    private:
        void HandleMousePicking(Window&                      window,
                                Camera&                      camera,
                                std::vector<std::shared_ptr<SceneModel>>& documents,
                                SelectionState&              selection,
                                const EngineConfig&          config);

        Ray  ScreenToWorldRay(double           mouseX,
                              double           mouseY,
                              int              screenW,
                              int              screenH,
                              const glm::mat4& view,
                              const glm::mat4& proj,
                              const glm::vec3& camPos);

    private:
        // --- Internal State ---
        NavigationMode m_navMode      = NavigationMode::CAD;

        double         m_lastMouseX   = 0.0;
        double         m_lastMouseY   = 0.0;

        // --- Input Tracking ---
        bool           m_mouseWasDown = false;
        bool           m_tabWasDown   = false;
        bool           m_lWasDown     = false;
        bool           m_fWasDown     = false;
        bool           m_hWasDown     = false;
        bool           m_uiWasDown    = false;
        bool           m_delWasDown   = false;
    };

} // namespace BimCore
