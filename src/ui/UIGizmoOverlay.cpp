// =============================================================================
// BimCore/apps/editor/ui/UIGizmoOverlay.cpp
// =============================================================================
#include "UIGizmoOverlay.h"
#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GLFW/glfw3.h> 

namespace BimCore {

    // Helper to project 3D world coordinates to 2D screen coordinates
    static bool WorldToScreen(const glm::vec3& worldPos, Camera* camera, int width, int height, glm::vec2& outScreenPos) {
        glm::mat4 viewProj = camera->GetProjectionMatrix() * camera->GetViewMatrix();
        glm::vec4 clipSpacePos = viewProj * glm::vec4(worldPos, 1.0f);
        
        if (clipSpacePos.w <= 0.0f) return false; 
        
        glm::vec3 ndcSpacePos = glm::vec3(clipSpacePos) / clipSpacePos.w;
        
        outScreenPos.x = ((ndcSpacePos.x + 1.0f) / 2.0f) * width;
        outScreenPos.y = ((1.0f - ndcSpacePos.y) / 2.0f) * height;
        return true;
    }

    void UIGizmoOverlay::Render(SelectionState& state, std::vector<std::shared_ptr<SceneModel>>& documents, Camera* camera, CommandHistory& history, GLFWwindow* window) {
        if (!camera || !window) return;
        
        int w, h;
        glfwGetWindowSize(window, &w, &h);
        ImDrawList* drawList = ImGui::GetBackgroundDrawList();

        // =========================================================================
        // 1. RENDER MEASUREMENT TOOL OVERLAYS (Lines & Snap Points)
        // =========================================================================
        if (state.activeTool == InteractionTool::Measure) {
            const ImU32 lineColor = IM_COL32(255, 165, 0, 255); // Orange
            const ImU32 textBgColor = IM_COL32(0, 0, 0, 180);
            const ImU32 textColor = IM_COL32(255, 255, 255, 255);
            const float lineThickness = 3.0f;

            auto drawMeasurement = [&](const glm::vec3& p1, const glm::vec3& p2) {
                glm::vec2 s1, s2;
                if (WorldToScreen(p1, camera, w, h, s1) && WorldToScreen(p2, camera, w, h, s2)) {
                    drawList->AddLine(ImVec2(s1.x, s1.y), ImVec2(s2.x, s2.y), lineColor, lineThickness);
                    drawList->AddCircleFilled(ImVec2(s1.x, s1.y), 4.0f, lineColor);
                    drawList->AddCircleFilled(ImVec2(s2.x, s2.y), 4.0f, lineColor);

                    float dist = glm::length(p1 - p2);
                    char distText[32];
                    snprintf(distText, sizeof(distText), "%.2f m", dist);

                    ImVec2 midPoint((s1.x + s2.x) * 0.5f, (s1.y + s2.y) * 0.5f);
                    ImVec2 textSize = ImGui::CalcTextSize(distText);
                    ImVec2 textPos(midPoint.x - textSize.x * 0.5f, midPoint.y - textSize.y * 0.5f);
                    
                    drawList->AddRectFilled(ImVec2(textPos.x - 4, textPos.y - 2), ImVec2(textPos.x + textSize.x + 4, textPos.y + textSize.y + 2), textBgColor, 4.0f);
                    drawList->AddText(textPos, textColor, distText);
                }
            };

            // Draw all completed measurements
            for (const auto& m : state.completedMeasurements) {
                drawMeasurement(m.p1, m.p2);
            }

            // Draw the active dynamic measurement line
            if (state.isMeasuringActive && state.isHoveringGeometry) {
                drawMeasurement(state.measureStartPoint, state.currentSnapPoint);
            }

            // Draw the Snap Indicators (Vertices, Edges, Faces)
            if (state.isHoveringGeometry) {
                glm::vec2 sp;
                if (WorldToScreen(state.currentSnapPoint, camera, w, h, sp)) {
                    if (state.currentSnapType == SnapType::Vertex) {
                        drawList->AddCircleFilled(ImVec2(sp.x, sp.y), 6.0f, IM_COL32(0, 255, 0, 255)); // Green dot
                    } 
                    else if (state.currentSnapType == SnapType::Edge) {
                        glm::vec2 e0, e1;
                        if (WorldToScreen(state.currentSnapEdgeV0, camera, w, h, e0) && WorldToScreen(state.currentSnapEdgeV1, camera, w, h, e1)) {
                            drawList->AddLine(ImVec2(e0.x, e0.y), ImVec2(e1.x, e1.y), IM_COL32(0, 255, 255, 255), 4.0f); // Cyan edge line
                        }
                        drawList->AddRectFilled(ImVec2(sp.x - 4, sp.y - 4), ImVec2(sp.x + 4, sp.y + 4), IM_COL32(0, 255, 255, 255)); // Cyan square
                    }
                    else if (state.currentSnapType == SnapType::Face) {
                        drawList->AddTriangleFilled(ImVec2(sp.x, sp.y - 6), ImVec2(sp.x - 6, sp.y + 4), ImVec2(sp.x + 6, sp.y + 4), IM_COL32(255, 255, 0, 255)); // Yellow triangle
                    }
                }
            }
        }

        // =========================================================================
        // 2. RENDER IMGUIZMO (Move/Rotate Tools)
        // =========================================================================
        if ((state.activeTool == InteractionTool::Move || state.activeTool == InteractionTool::Rotate) && state.explodeFactor <= 0.01f && !state.objects.empty() && !documents.empty()) {
            
            glm::vec3 selectionCenter(0.0f);
            int validObjects = 0;
            
            struct SelItem { std::shared_ptr<SceneModel> doc; const RenderSubMesh* sub; };
            std::vector<SelItem> activeItems;
            
            for (const auto& obj : state.objects) {
                for (auto& doc : documents) {
                    const auto& subMeshes = doc->GetGeometry().subMeshes;
                    auto it = std::find_if(subMeshes.begin(), subMeshes.end(), [&](const RenderSubMesh& s) { return s.guid == obj.guid; });
                    
                    if (it != subMeshes.end()) {
                        glm::mat4 objMat = doc->GetObjectTransform(obj.guid);
                        glm::vec4 worldCenter = objMat * glm::vec4(it->center[0], it->center[1], it->center[2], 1.0f);
                        selectionCenter += glm::vec3(worldCenter);
                        activeItems.push_back({doc, &(*it)});
                        validObjects++;
                        break;
                    }
                }
            }

            if (validObjects > 0) {
                selectionCenter /= (float)validObjects;
                
                static glm::mat4 currentGizmoMatrix(1.0f);
                if (!ImGuizmo::IsUsing()) currentGizmoMatrix = glm::translate(glm::mat4(1.0f), selectionCenter);

                ImGuizmo::SetOrthographic(false);
                ImGuizmo::SetDrawlist(drawList);
                ImGuizmo::SetRect(0, 0, (float)w, (float)h);

                glm::mat4 viewMatrix = camera->GetViewMatrix();
                glm::mat4 projMatrix = camera->GetProjectionMatrix();
                glm::mat4 deltaMatrix(1.0f);

                ImGuizmo::OPERATION currentOp = (state.activeTool == InteractionTool::Rotate) ? ImGuizmo::ROTATE : ImGuizmo::TRANSLATE;
                ImGuizmo::MODE      currentMode = (validObjects > 1) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;

                static bool wasUsingGizmo = false;
                static std::vector<CmdTransform::TransformData> dragData;

                bool isUsingGizmo = ImGuizmo::IsUsing();

                if (isUsingGizmo && !wasUsingGizmo) {
                    dragData.clear();
                    for (auto& item : activeItems) {
                        CmdTransform::TransformData td; td.doc = item.doc; td.guid = item.sub->guid; td.oldTransform = item.doc->GetObjectTransform(item.sub->guid);
                        dragData.push_back(td);
                    }
                }

                bool manipulated = ImGuizmo::Manipulate(glm::value_ptr(viewMatrix), glm::value_ptr(projMatrix), currentOp, currentMode, glm::value_ptr(currentGizmoMatrix), glm::value_ptr(deltaMatrix));

                if (manipulated) {
                    for (auto& item : activeItems) {
                        glm::mat4 currentTransform = item.doc->GetObjectTransform(item.sub->guid);
                        item.doc->SetObjectTransform(item.sub->guid, deltaMatrix * currentTransform);
                    }
                    state.updateGeometry = true; 
                }

                if (!isUsingGizmo && wasUsingGizmo && !dragData.empty()) {
                    for (auto& td : dragData) td.newTransform = td.doc->GetObjectTransform(td.guid);
                    history.ExecuteCommand(std::make_unique<CmdTransform>(state, dragData));
                    dragData.clear();
                }

                wasUsingGizmo = isUsingGizmo;
            }
        }
    }
} // namespace BimCore