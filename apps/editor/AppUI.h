// =============================================================================
// BimCore/apps/editor/AppUI.h
// =============================================================================
#pragma once
#include "ui/UIState.h"
#include "ui/UIStatusOverlay.h"
#include "ui/UIMainPanel.h"
#include "ui/UIPropertiesPanel.h"

#include "scene/Camera.h"
#include "graphics/GraphicsContext.h"
#include <vector>
#include <memory>

namespace BimCore {

    class AppUI {
    public:
        SelectionState state;

        void NewFrame();
        void Render(SelectionState&              selection,
                    GraphicsContext&             graphics,
                    std::vector<std::shared_ptr<BimDocument>>& documents,
                    Camera&                      camera,
                    float                        configMaxExplode,
                    bool&                        triggerFocus,
                    bool                         isFlightMode,
                    bool&                        triggerRebuild);

    private:
        UIStatusOverlay   m_overlay;
        UIMainPanel       m_mainPanel;
        UIPropertiesPanel m_propertiesPanel;
    };

} // namespace BimCore
