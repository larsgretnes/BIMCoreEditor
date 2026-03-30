// =============================================================================
// BimCore/apps/editor/ui/AppUI.cpp
// =============================================================================
#include "ui/AppUI.h"
#include "ui/UICommandPanel.h"
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_wgpu.h>
#include <imgui.h>

namespace BimCore {

    void AppUI::NewFrame() {
        ImGui_ImplWGPU_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void AppUI::Render(SelectionState& selection, GraphicsContext& graphics, std::vector<std::shared_ptr<SceneModel>>& documents, Camera& camera, EngineConfig& config, bool& triggerFocus, bool isFlightMode, bool& triggerRebuild, CommandHistory* history, GLFWwindow* window) {

        m_overlay.RenderFlyMode(isFlightMode);

        if (!state.showUI) {
            ImGui::Render();
            return;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));

        bool editingActiveAtStartOfFrame = !state.activeEditGuid.empty();

        UIMainPanel::Render(state, documents, config.MaxExplodeFactor, triggerFocus, triggerRebuild, &camera, *history, window);
        
        m_overlay.RenderStatusPanel(state, documents);
        m_propertiesPanel.Render(state, documents, triggerFocus, *history);
        m_overlay.RenderContextMenu(state, triggerFocus, *history);

        UICommandPanel::Render(state, config, triggerFocus, triggerRebuild);

        // --- OPPDATERT: Hierarkisk Escape-håndtering ---
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) && !editingActiveAtStartOfFrame) {
            if (state.showCommandPanel) {
                // Lukk kommandopanelet først hvis det er åpent
                state.showCommandPanel = false;
            } else {
                // Hvis panelet ikke er åpent, fjern valgte objekter som normalt
                state.objects.clear();
            }
        }

        ImGui::PopStyleVar();
        ImGui::Render();
    }

} // namespace BimCore