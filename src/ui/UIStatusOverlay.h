// =============================================================================
// BimCore/apps/editor/ui/UIStatusOverlay.h
// =============================================================================
#pragma once

#include "UIState.h"
#include <memory>
#include <vector>

namespace BimCore {

    class UIStatusOverlay {
    public:
        void RenderFlyMode(bool isFlightMode);
        void RenderStatusPanel(SelectionState& state, std::vector<std::shared_ptr<SceneModel>>& documents);
        void RenderContextMenu(SelectionState& state, bool& triggerFocus);
    };

} // namespace BimCore
