// =============================================================================
// BimCore/apps/editor/ui/UIMainPanel.h
// =============================================================================
#pragma once
#include "UIState.h"
#include <memory>

namespace BimCore {
    class UIMainPanel {
    public:
        void Render(SelectionState& state, std::shared_ptr<BimDocument> document, float configMaxExplode, bool& triggerFocus);
    private:
        void DrawResetModal(SelectionState& state, std::shared_ptr<BimDocument> document);
        void HandleShiftSelection(SelectionState& state, int visualIdx, uint32_t meshIdx, const std::string& groupName, const std::vector<uint32_t>& currentArray, std::shared_ptr<BimDocument> document);
    };
}
