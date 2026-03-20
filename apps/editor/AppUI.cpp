// =============================================================================
// BimCore/apps/editor/AppUI.cpp
// =============================================================================
#include "AppUI.h"
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_wgpu.h>
#include <imgui.h>

namespace BimCore {

    void AppUI::NewFrame() {
        ImGui_ImplWGPU_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void AppUI::Render(SelectionState& selection, GraphicsContext& graphics, std::shared_ptr<BimDocument> document, Camera& camera, float configMaxExplode, bool& triggerFocus, bool isFlightMode) {

        m_overlay.RenderFlyMode(isFlightMode);

        if (!state.showUI) {
            ImGui::Render();
            return;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));

        bool editingActiveAtStartOfFrame = !state.activeEditGuid.empty();

        m_mainPanel.Render(state, document, configMaxExplode, triggerFocus);
        m_overlay.RenderStatusPanel(state, document);
        m_propertiesPanel.Render(state, document, triggerFocus);
        m_overlay.RenderContextMenu(state, triggerFocus);

        if (ImGui::IsKeyPressed(ImGuiKey_Escape) && !editingActiveAtStartOfFrame) {
            state.objects.clear();
        }

        ImGui::PopStyleVar();
        ImGui::Render();
    }

} // namespace BimCore
