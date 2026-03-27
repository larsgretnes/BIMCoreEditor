// =============================================================================
// BimCore/apps/editor/ui/AppUI.cpp
// =============================================================================
#include "ui/AppUI.h"
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_wgpu.h>
#include <imgui.h>

namespace BimCore {

    void AppUI::NewFrame() {
        ImGui_ImplWGPU_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void AppUI::Render(SelectionState& selection, GraphicsContext& graphics, std::vector<std::shared_ptr<SceneModel>>& documents, Camera& camera, float configMaxExplode, bool& triggerFocus, bool isFlightMode, bool& triggerRebuild, CommandHistory* history, GLFWwindow* window) {

        m_overlay.RenderFlyMode(isFlightMode);

        if (!state.showUI) {
            ImGui::Render();
            return;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));

        bool editingActiveAtStartOfFrame = !state.activeEditGuid.empty();

        UIMainPanel::Render(state, documents, configMaxExplode, triggerFocus, triggerRebuild, &camera, *history, window);
        
        m_overlay.RenderStatusPanel(state, documents);
        m_propertiesPanel.Render(state, documents, triggerFocus, *history);
        
        // Pass the history object to the context menu!
        m_overlay.RenderContextMenu(state, triggerFocus, *history);

        if (ImGui::IsKeyPressed(ImGuiKey_Escape) && !editingActiveAtStartOfFrame) {
            state.objects.clear();
        }

        ImGui::PopStyleVar();
        ImGui::Render();
    }

} // namespace BimCore