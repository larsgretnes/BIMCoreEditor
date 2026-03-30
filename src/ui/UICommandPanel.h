// =============================================================================
// BimCore/apps/editor/ui/UICommandPanel.h
// =============================================================================
#pragma once
#include "ui/UIState.h"
#include "core/EngineConfig.h"
#include <string>
#include <vector>
#include <imgui.h> // <--- INKLUDERT FOR NYESTE VERSJON

namespace BimCore {

    class UICommandPanel {
    public:
        static void Render(SelectionState& state, EngineConfig& config, bool& triggerFocus, bool& triggerRebuild);
        
        static std::vector<std::string> TokenizeCommand(const std::string& input);
        static void ExecuteCommand(const std::string& input, SelectionState& state, EngineConfig& config, bool& triggerFocus, bool& triggerRebuild);

    private:
        // <--- BRUKER KORREKT STRUKTUR FOR NYESTE VERSJON AV IMGUI
        static int TextEditCallbackStub(ImGuiInputTextCallbackData* data);
        static void BuildCommandList();

        static std::vector<std::string> s_commandLog;
        static std::vector<std::string> s_commandHistory;
        static int s_historyPos;
        static std::vector<std::string> s_availableCommands;
    };

} // namespace BimCore