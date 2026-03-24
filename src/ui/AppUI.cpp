// =============================================================================
// BimCore/apps/editor/AppUI.cpp
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

    // --- FIXED: Accept CommandHistory* history ---
    void AppUI::Render(SelectionState& selection, GraphicsContext& graphics, std::vector<std::shared_ptr<SceneModel>>& documents, Camera& camera, float configMaxExplode, bool& triggerFocus, bool isFlightMode, bool& triggerRebuild, CommandHistory* history) {

        m_overlay.RenderFlyMode(isFlightMode);

        if (!state.showUI) {
            ImGui::Render();
            return;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));

        bool editingActiveAtStartOfFrame = !state.activeEditGuid.empty();

        // --- FIXED: Pass the history object down to the Main Panel ---
        m_mainPanel.Render(state, documents, configMaxExplode, triggerFocus, triggerRebuild, &camera, *history);
        
        m_overlay.RenderStatusPanel(state, documents);
        m_propertiesPanel.Render(state, documents, triggerFocus);
        m_overlay.RenderContextMenu(state, triggerFocus);

        if (ImGui::IsKeyPressed(ImGuiKey_Escape) && !editingActiveAtStartOfFrame) {
            state.objects.clear();
        }

        ImGui::PopStyleVar();
        ImGui::Render();
    }

} // namespace BimCore