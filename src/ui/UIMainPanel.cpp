// =============================================================================
// BimCore/apps/editor/ui/UIMainPanel.cpp
// =============================================================================
#include "UIMainPanel.h"
#include "UIToolbar.h"
#include "UISearchPanel.h"
#include "UIModelTree.h"
#include "UIGizmoOverlay.h"
#include <imgui.h>
#include <ImGuizmo.h>

namespace BimCore {

    void UIMainPanel::DrawResetModal(SelectionState& state, std::vector<std::shared_ptr<SceneModel>>& documents, bool& triggerRebuild, CommandHistory& history) {
        if (ImGui::BeginPopupModal("Reset Model", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("This will roll back all modifications.\nAre you sure?");
            ImGui::Separator();

            if (ImGui::Button("OK", ImVec2(120, 0))) {

                while(history.CanUndo()) history.Undo();

                for (auto& doc : documents) {
                    doc->SetHidden(false); 
                    for (auto& [guid, props] : state.originalProperties) {
                        for (auto& [k, v] : props) doc->UpdateElementProperty(guid, k, v);
                    }
                }
                
                triggerRebuild = true;
                state.explodeFactor = 0.0f;
                state.updateGeometry = true; 
                
                state.clipXMin = -1e9f; state.clipXMax = 1e9f;
                state.clipYMin = -1e9f; state.clipYMax = 1e9f;
                state.clipZMin = -1e9f; state.clipZMax = 1e9f;

                state.showPlaneXMin = false; state.showPlaneXMax = false;
                state.showPlaneYMin = false; state.showPlaneYMax = false;
                state.showPlaneZMin = false; state.showPlaneZMax = false;

                memset(state.globalSearchBuf, 0, sizeof(state.globalSearchBuf));
                memset(state.localSearchBuf, 0, sizeof(state.localSearchBuf));

                state.originalProperties.clear(); state.deletedProperties.clear();
                state.hiddenObjects.clear(); state.objects.clear();
                state.hiddenStateChanged = true; state.triggerResetCamera = true; state.selectionChanged = true;
                state.measureToolActive = false; state.completedMeasurements.clear(); state.isMeasuringActive = false;

                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
    }

    void UIMainPanel::Render(SelectionState& state, std::vector<std::shared_ptr<SceneModel>>& documents, float configMaxExplode, bool& triggerFocus, bool& triggerRebuild, Camera* camera, CommandHistory& history, GLFWwindow* window) {
        
        ImGuizmo::BeginFrame();

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float statsPanelHeight = 75.0f;
        const float mainPanelHeight = viewport->WorkSize.y - statsPanelHeight;

        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y), ImGuiCond_Always);
        ImGui::SetNextWindowSizeConstraints(ImVec2(300, mainPanelHeight), ImVec2(viewport->WorkSize.x / 2.0f, mainPanelHeight));
        ImGui::SetNextWindowSize(ImVec2(400.0f, mainPanelHeight), ImGuiCond_FirstUseEver);

        ImGui::Begin("Main Menu", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

        // 1. Render Toolbar
        UIToolbar::Render(state, documents, configMaxExplode, history, triggerRebuild);
        
        // 2. Render Modal (Dette er det eneste stedet den skal kalles!)
        DrawResetModal(state, documents, triggerRebuild, history);

        // 3. Render Search Panel (If docs exist)
        if (documents.empty()) {
            ImGui::TextDisabled("No models loaded.");
        } else {
            UISearchPanel::Render(state, documents, triggerFocus);
            UIModelTree::Render(state, documents, triggerFocus, triggerRebuild);
        }

        ImGui::End();

        // 4. Render 3D Canvas Overlays
        UIGizmoOverlay::Render(state, documents, camera, history, window);
    }
} // namespace BimCore