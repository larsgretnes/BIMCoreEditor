// =============================================================================
// BimCore/apps/editor/ui/UIGizmoOverlay.h
// =============================================================================
#pragma once
#include "ui/UIState.h"
#include "scene/SceneModel.h"
#include "graphics/Camera.h" 
#include "core/CommandHistory.h"
#include <vector>
#include <memory>

struct GLFWwindow;

namespace BimCore {
    class UIGizmoOverlay {
    public:
        static void Render(SelectionState& state, std::vector<std::shared_ptr<SceneModel>>& documents, Camera* camera, CommandHistory& history, GLFWwindow* window);
    };
} // namespace BimCore