// =============================================================================
// BimCore/apps/editor/AppUI.h
// =============================================================================
#pragma once
#include "ui/UIState.h"
#include "ui/UIStatusOverlay.h"
#include "ui/UIMainPanel.h"
#include "ui/UIPropertiesPanel.h"

// --- NEW: Restore the missing includes for the Render signature! ---
#include "scene/Camera.h"
#include "graphics/GraphicsContext.h"

namespace BimCore {

    class AppUI {
    public:
        SelectionState state;

        void NewFrame();
        void Render(SelectionState&              selection,
                    GraphicsContext&             graphics,
                    std::shared_ptr<BimDocument> document,
                    Camera&                      camera,
                    float                        configMaxExplode,
                    bool&                        triggerFocus,
                    bool                         isFlightMode);

    private:
        UIStatusOverlay   m_overlay;
        UIMainPanel       m_mainPanel;
        UIPropertiesPanel m_propertiesPanel;
    };

} // namespace BimCore
