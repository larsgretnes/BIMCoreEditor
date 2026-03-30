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
#include "core/CommandHistory.h"

namespace BimCore {

    enum class NavigationMode {
        Flight,
        CAD
    };

    enum class DraggedPlane { 
        None, XMin, XMax, YMin, YMax, ZMin, ZMax 
    };

    class InputController {
    public:
        void Update(Window&                                     window,
                    Camera&                                     camera,
                    std::vector<std::shared_ptr<SceneModel>>&   documents,
                    SelectionState&                             selection,
                    const EngineConfig&                         config,
                    float                                       deltaTime,
                    uint32_t&                                   currentLightingMode,
                    bool&                                       triggerFocus,
                    CommandHistory&                             history);

        bool IsFlightMode() const;
        DraggedPlane CheckPlaneHits(const Ray& ray, const SelectionState& state, bool showClips, glm::vec3& outHitPoint);

    private:
        void HandleMousePicking(Window&                                     window,
                                Camera&                                     camera,
                                std::vector<std::shared_ptr<SceneModel>>&   documents,
                                SelectionState&                             selection,
                                const EngineConfig&                         config,
                                CommandHistory&                             history);

        Ray  ScreenToWorldRay(double           mouseX,
                              double           mouseY,
                              int              screenW,
                              int              screenH,
                              const glm::mat4& view,
                              const glm::mat4& proj,
                              const glm::vec3& camPos);

    private:
        NavigationMode m_navMode      = NavigationMode::CAD;

        double         m_lastMouseX   = 0.0;
        double         m_lastMouseY   = 0.0;

        bool           m_mouseWasDown = false;
        bool           m_tabWasDown   = false;
        bool           m_lWasDown     = false;
        bool           m_fWasDown     = false;
        bool           m_hWasDown     = false;
        bool           m_uiWasDown    = false;
        bool           m_delWasDown   = false;
        bool           m_f3WasDown    = false;
        bool           m_cmdWasDown   = false;

        DraggedPlane   m_draggedPlane = DraggedPlane::None;
        glm::vec3      m_dragStartPoint {0.0f};
        float          m_dragStartClipValue = 0.0f;
    };

} // namespace BimCore