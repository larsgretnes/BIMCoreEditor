// =============================================================================
// BimCore/apps/editor/ui/UICommandPanel.cpp
// =============================================================================
#include "UICommandPanel.h"
#include <imgui.h>
#include <sstream>
#include <algorithm>

namespace BimCore {

    std::vector<std::string> UICommandPanel::s_commandLog;
    std::vector<std::string> UICommandPanel::s_commandHistory;
    int UICommandPanel::s_historyPos = -1;
    std::vector<std::string> UICommandPanel::s_availableCommands = {
        "clear", "help", "focus", "select_all", "hide_selection", "set_speed", "set_zoom_speed", "explode"
    };

    std::vector<std::string> UICommandPanel::TokenizeCommand(const std::string& input) {
        std::vector<std::string> tokens;
        std::istringstream stream(input);
        std::string token;
        while (stream >> token) {
            tokens.push_back(token);
        }
        return tokens;
    }

    void UICommandPanel::ExecuteCommand(const std::string& input, SelectionState& state, EngineConfig& config, bool& triggerFocus, bool& triggerRebuild) {
        if (input.empty()) return;

        s_commandLog.push_back("> " + input);
        s_commandHistory.push_back(input);
        s_historyPos = -1;

        std::vector<std::string> tokens = TokenizeCommand(input);
        if (tokens.empty()) return;

        std::string cmd = tokens[0];
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

        if (cmd == "clear") {
            s_commandLog.clear();
        } 
        else if (cmd == "help") {
            s_commandLog.push_back("Available Commands: clear, help, focus, select_all, hide_selection, set_speed <val>, set_zoom_speed <val>, explode <val>");
        }
        else if (cmd == "focus") {
            triggerFocus = true;
            s_commandLog.push_back("Camera focused on selection.");
        }
        else if (cmd == "hide_selection") {
            for (auto& o : state.objects) state.hiddenObjects.insert(o.guid);
            state.objects.clear();
            state.hiddenStateChanged = true;
            s_commandLog.push_back("Selection hidden.");
        }
        else if (cmd == "set_speed") {
            if (tokens.size() > 1) {
                try {
                    config.BaseSpeed = std::stof(tokens[1]);
                    s_commandLog.push_back("BaseSpeed set to " + tokens[1]);
                } catch (...) { s_commandLog.push_back("Error: Invalid numeric value."); }
            } else { s_commandLog.push_back("Usage: set_speed <value>"); }
        }
        else if (cmd == "set_zoom_speed") {
            if (tokens.size() > 1) {
                try {
                    config.ZoomSpeed = std::stof(tokens[1]);
                    s_commandLog.push_back("ZoomSpeed set to " + tokens[1]);
                } catch (...) { s_commandLog.push_back("Error: Invalid numeric value."); }
            } else { s_commandLog.push_back("Usage: set_zoom_speed <value>"); }
        }
        else if (cmd == "explode") {
            if (tokens.size() > 1) {
                try {
                    state.explodeFactor = std::stof(tokens[1]);
                    state.updateGeometry = true;
                    s_commandLog.push_back("Explode factor set to " + tokens[1]);
                } catch (...) { s_commandLog.push_back("Error: Invalid numeric value."); }
            } else { s_commandLog.push_back("Usage: explode <value>"); }
        }
        else {
            s_commandLog.push_back("Unknown command: '" + cmd + "'. Type 'help' for options.");
        }
    }

    int UICommandPanel::TextEditCallbackStub(ImGuiInputTextCallbackData* data) {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
            std::string input(data->Buf, data->BufTextLen);
            std::vector<std::string> matches;
            for (const auto& cmd : s_availableCommands) {
                if (cmd.find(input) == 0) matches.push_back(cmd);
            }
            if (matches.size() == 1) {
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, matches[0].c_str());
                data->InsertChars(data->BufTextLen, " ");
            } else if (!matches.empty()) {
                std::string matchStr = "Matches: ";
                for (const auto& m : matches) matchStr += m + " ";
                s_commandLog.push_back(matchStr);
            }
        }
        else if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
            const int prev_history_pos = s_historyPos;
            if (data->EventKey == ImGuiKey_UpArrow) {
                if (s_historyPos == -1) s_historyPos = (int)s_commandHistory.size() - 1;
                else if (s_historyPos > 0) s_historyPos--;
            } else if (data->EventKey == ImGuiKey_DownArrow) {
                if (s_historyPos != -1) {
                    if (++s_historyPos >= (int)s_commandHistory.size()) s_historyPos = -1;
                }
            }

            if (prev_history_pos != s_historyPos) {
                std::string history_str = (s_historyPos >= 0) ? s_commandHistory[s_historyPos] : "";
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, history_str.c_str());
            }
        }
        return 0;
    }

    void UICommandPanel::Render(SelectionState& state, EngineConfig& config, bool& triggerFocus, bool& triggerRebuild) {
        if (!state.showCommandPanel) return;

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        
        // Dynamisk utregning basert på panelene på sidene!
        float startX = viewport->WorkPos.x + state.uiMainPanelWidth;
        float startY = viewport->WorkPos.y;
        float width = viewport->WorkSize.x - state.uiMainPanelWidth - state.uiPropertiesPanelWidth;

        // Lås posisjon helt fast
        ImGui::SetNextWindowPos(ImVec2(startX, startY), ImGuiCond_Always);
        
        // Constraints: Min bredde = Max bredde (låser bredden). Min høyde 100, Max høyde til bunn av skjermen.
        ImGui::SetNextWindowSizeConstraints(ImVec2(width, 100.0f), ImVec2(width, viewport->WorkSize.y));
        ImGui::SetNextWindowSize(ImVec2(width, 150.0f), ImGuiCond_FirstUseEver);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.1f, 0.95f)); 
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        
        // Fjernet NoResize flagget, så nå kan du ta tak i bunnen av panelet og dra det ned!
        ImGui::Begin("Command Terminal", &state.showCommandPanel, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);

        // --- VIKTIG: Lagre høyden på panelet slik at Search-panelet vet hvor langt nede det skal starte ---
        state.uiCommandPanelHeight = ImGui::GetWindowSize().y;

        // Render Log
        ImGui::BeginChild("ScrollingRegion", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), false, ImGuiWindowFlags_HorizontalScrollbar);
        for (const auto& log : s_commandLog) {
            ImGui::TextUnformatted(log.c_str());
        }
        
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f); 
        }
        ImGui::EndChild();

        ImGui::Separator();

        // Input Field
        static char inputBuf[256] = "";
        
        ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;
        
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();

        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputText("##CommandInput", inputBuf, IM_ARRAYSIZE(inputBuf), inputFlags, &TextEditCallbackStub)) {
            ExecuteCommand(inputBuf, state, config, triggerFocus, triggerRebuild);
            inputBuf[0] = '\0';
            ImGui::SetKeyboardFocusHere(-1); 
        }

        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

} // namespace BimCore