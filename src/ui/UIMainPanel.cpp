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

                // 1. Undo all command history
                while(history.CanUndo()) history.Undo();

                float minB[3] = {1e9f, 1e9f, 1e9f};
                float maxB[3] = {-1e9f, -1e9f, -1e9f};

                // 2. Restore properties, visibility, and FORCE geometry pull-back
                for (auto& doc : documents) {
                    doc->SetHidden(false); 
                    for (auto& [guid, props] : state.originalProperties) {
                        for (auto& [k, v] : props) doc->UpdateElementProperty(guid, k, v);
                    }
                    
                    auto& geom = doc->GetGeometry();
                    
                    // Directly overwrite the exploded vertices with the originals
                    if (!geom.originalVertices.empty()) {
                        geom.vertices = geom.originalVertices;
                    }
                    
                    // Calculate the TRUE bounds of the model directly from the data
                    for(int i=0; i<3; ++i) {
                        if (geom.minBounds[i] < minB[i]) minB[i] = geom.minBounds[i];
                        if (geom.maxBounds[i] > maxB[i]) maxB[i] = geom.maxBounds[i];
                    }
                }
                
                // 3. Reset Explode and Rebuild triggers
                triggerRebuild = true;
                state.explodeFactor = 0.0f;
                state.updateGeometry = true; 
                
                // 4. Snap Clipping Planes to the calculated true bounds
                if (!documents.empty()) {
                    state.clipXMin = minB[0]; state.clipXMax = maxB[0];
                    state.clipYMin = minB[1]; state.clipYMax = maxB[1];
                    state.clipZMin = minB[2]; state.clipZMax = maxB[2];
                }

                state.showPlaneXMin = false; state.showPlaneXMax = false;
                state.showPlaneYMin = false; state.showPlaneYMax = false;
                state.showPlaneZMin = false; state.showPlaneZMax = false;

                // 5. Clear Search Buffers
                memset(state.globalSearchBuf, 0, sizeof(state.globalSearchBuf));
                memset(state.localSearchBuf, 0, sizeof(state.localSearchBuf));

                // 6. Clear Selection, Hides, Deletes, and Property tracking
                state.originalProperties.clear(); 
                state.deletedProperties.clear();
                
                state.hiddenObjects.clear(); 
                state.deletedObjects.clear(); 
                state.objects.clear();
                
                // 7. Reset Measurements
                state.measureToolActive = false; 
                state.completedMeasurements.clear(); 
                state.isMeasuringActive = false;

                // 8. Trigger UI & Camera updates
                state.hiddenStateChanged = true; 
                state.triggerResetCamera = true; 
                state.selectionChanged = true;

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
        
        // 2. Render Modal 
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