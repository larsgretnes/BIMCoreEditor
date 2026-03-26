// =============================================================================
// BimCore/apps/editor/ui/UIToolbar.h
// =============================================================================
#pragma once
#include "ui/UIState.h"
#include "scene/SceneModel.h"
#include "core/CommandHistory.h"
#include <vector>
#include <memory>

namespace BimCore {
    class UIToolbar {
    public:
        static void Render(SelectionState& state, std::vector<std::shared_ptr<SceneModel>>& documents, float configMaxExplode, CommandHistory& history, bool& triggerRebuild);
    };
} // namespace BimCore