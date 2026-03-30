// =============================================================================
// BimCore/apps/editor/ui/UISearchPanel.h
// =============================================================================
#pragma once
#include "ui/UIState.h"
#include "scene/SceneModel.h"
#include <vector>
#include <memory>
#include <string>

namespace BimCore {
    class UISearchPanel {
    public:
        static void Render(SelectionState& state, std::vector<std::shared_ptr<SceneModel>>& documents, bool& triggerFocus);
        static void ExecuteTextSearch(const std::string& query, std::vector<std::shared_ptr<SceneModel>>& documents, SelectionState& state);
        
        // Eksponert for Catch2 Unit Testing: 
        // Deler opp "nøkkel:verdi" og håndterer anførselstegn (quotes) og wildcards (*).
        static void ParseSearchQuery(const std::string& query, std::string& outKey, std::string& outValue);
    };
} // namespace BimCore