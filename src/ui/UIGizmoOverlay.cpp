// =============================================================================
// BimCore/apps/editor/ui/UIGizmoOverlay.cpp
// =============================================================================
#include "UIGizmoOverlay.h"
#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GLFW/glfw3.h> // For getting window size

namespace BimCore {

    void UIGizmoOverlay::Render(SelectionState& state, std::vector<std::shared_ptr<SceneModel>>& documents, Camera* camera, CommandHistory& history, GLFWwindow* window) {
        if (state.activeTool != InteractionTool::Select && state.explodeFactor <= 0.01f && !state.objects.empty() && !documents.empty() && camera != nullptr && window != nullptr) {
            
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

                int w, h;
                glfwGetWindowSize(window, &w, &h);

                ImGuizmo::SetOrthographic(false);
                ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
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