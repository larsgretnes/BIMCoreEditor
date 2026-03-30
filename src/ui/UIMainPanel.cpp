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

#define ICON_FA_TIMES "\xef\x80\x8d" 
#define ICON_FA_SEARCH "\xef\x80\x82" 

namespace BimCore {

    void UIMainPanel::DrawResetModal(SelectionState& state, std::vector<std::shared_ptr<SceneModel>>& documents, bool& triggerRebuild, CommandHistory& history) {
        if (ImGui::BeginPopupModal("Reset Model", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("This will roll back all modifications.\nAre you sure?");
            ImGui::Separator();

            if (ImGui::Button("OK", ImVec2(120, 0))) {

                while(history.CanUndo()) history.Undo();

                float minB[3] = {1e9f, 1e9f, 1e9f};
                float maxB[3] = {-1e9f, -1e9f, -1e9f};

                for (auto& doc : documents) {
                    doc->SetHidden(false); 
                    for (auto& [guid, props] : state.originalProperties) {
                        for (auto& [k, v] : props) doc->UpdateElementProperty(guid, k, v);
                    }
                    
                    auto& geom = doc->GetGeometry();
                    
                    if (!geom.originalVertices.empty()) {
                        geom.vertices = geom.originalVertices;
                    }
                    
                    for(int i=0; i<3; ++i) {
                        if (geom.minBounds[i] < minB[i]) minB[i] = geom.minBounds[i];
                        if (geom.maxBounds[i] > maxB[i]) maxB[i] = geom.maxBounds[i];
                    }
                }
                
                triggerRebuild = true;
                state.explodeFactor = 0.0f;
                state.updateGeometry = true; 
                
                if (!documents.empty()) {
                    state.clipXMin = minB[0]; state.clipXMax = maxB[0];
                    state.clipYMin = minB[1]; state.clipYMax = maxB[1];
                    state.clipZMin = minB[2]; state.clipZMax = maxB[2];
                }

                state.showPlaneXMin = false; state.showPlaneXMax = false;
                state.showPlaneYMin = false; state.showPlaneYMax = false;
                state.showPlaneZMin = false; state.showPlaneZMax = false;

                memset(state.globalSearchBuf, 0, sizeof(state.globalSearchBuf));
                memset(state.localSearchBuf, 0, sizeof(state.localSearchBuf));

                state.originalProperties.clear(); 
                state.deletedProperties.clear();
                
                state.hiddenObjects.clear(); 
                state.deletedObjects.clear(); 
                state.objects.clear();
                
                state.measureToolActive = false; 
                state.completedMeasurements.clear(); 
                state.isMeasuringActive = false;

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

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float statsPanelHeight = 75.0f;
        const float mainPanelHeight = viewport->WorkSize.y - statsPanelHeight;

        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y), ImGuiCond_Always);
        ImGui::SetNextWindowSizeConstraints(ImVec2(300, mainPanelHeight), ImVec2(viewport->WorkSize.x / 2.0f, mainPanelHeight));
        ImGui::SetNextWindowSize(ImVec2(400.0f, mainPanelHeight), ImGuiCond_FirstUseEver);

        ImGui::Begin("Main Menu", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);
        
        // --- LAGRE MAIN PANEL BREDDEN TIL STATE ---
        state.uiMainPanelWidth = ImGui::GetWindowSize().x; 

        UIToolbar::Render(state, documents, configMaxExplode, history, triggerRebuild);

        ImGui::Spacing();
        if (ImGui::Button(ICON_FA_SEARCH " Advanced Search (F3)", ImVec2(-FLT_MIN, 0))) {
            state.showSearchPanel = !state.showSearchPanel;
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        if (!state.objects.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.4f, 0.4f, 1.0f));
            
            if (ImGui::Button(ICON_FA_TIMES " Clear Active Selection", ImVec2(-FLT_MIN, 0))) {
                state.objects.clear();
                state.selectionChanged = true;
            }
            
            ImGui::PopStyleColor(3);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }

        DrawResetModal(state, documents, triggerRebuild, history);

        if (documents.empty()) {
            ImGui::TextDisabled("No models loaded.");
        } else {
            UIModelTree::Render(state, documents, triggerFocus, triggerRebuild);
        }

        ImGui::End();

        // --- RENDER SEARCH PANEL DYNAMISK ---
        if (!documents.empty() && state.showSearchPanel) {
            
            // Regn ut hvor panelet skal starte basert på Command Panelet
            float searchY = viewport->WorkPos.y;
            if (state.showCommandPanel) {
                searchY += state.uiCommandPanelHeight; // Skyves under Command Panelet
            }
            
            // Lås posisjonen fast inntil høyrekanten på Main Menu
            ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + state.uiMainPanelWidth, searchY), ImGuiCond_Always);
            ImGui::SetNextWindowSizeConstraints(ImVec2(350, 200), ImVec2(viewport->WorkSize.x / 2.0f, viewport->WorkSize.y));
            ImGui::SetNextWindowSize(ImVec2(400.0f, mainPanelHeight), ImGuiCond_FirstUseEver);
            
            UISearchPanel::Render(state, documents, triggerFocus);
        }

        UIGizmoOverlay::Render(state, documents, camera, history, window);
    }
} // namespace BimCore