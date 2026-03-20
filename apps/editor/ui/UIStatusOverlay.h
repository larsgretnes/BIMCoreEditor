// =============================================================================
// BimCore/apps/editor/ui/UIStatusOverlay.h
// =============================================================================
#pragma once

#include "UIState.h"
#include <memory>

namespace BimCore {

    class UIStatusOverlay {
    public:
        void RenderFlyMode(bool isFlightMode);
        void RenderStatusPanel(SelectionState& state, std::shared_ptr<BimDocument> document);
        void RenderContextMenu(SelectionState& state, bool& triggerFocus);
    };

} // namespace BimCore
