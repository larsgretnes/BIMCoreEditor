// =============================================================================
// BimCore/apps/editor/InputController.cpp
// =============================================================================
#include "input/InputController.h"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <ImGuizmo.h>
#include <ifcparse/IfcFile.h>
#include <algorithm>
#include <cmath>
#include "core/Core.h"
#include "core/CommandHistory.h"

namespace BimCore {

    bool InputController::IsFlightMode() const {
        return m_navMode == NavigationMode::Flight;
    }

    DraggedPlane InputController::CheckPlaneHits(const Ray& ray, const SelectionState& state, bool showClips, glm::vec3& outHitPoint) {
        if (!showClips) return DraggedPlane::None;

        DraggedPlane bestPlane = DraggedPlane::None;
        float bestDist = 1e9f;

        auto testPlane = [&](DraggedPlane plane, float planeVal, int axis, int uAxis, int vAxis) {
            if (std::abs(ray.direction[axis]) < 1e-6f) return; 
            
            float t = (planeVal - ray.origin[axis]) / ray.direction[axis];
            if (t > 0.0f && t < bestDist) {
                glm::vec3 p = ray.origin + ray.direction * t;
                
                float padU = (state.sceneMaxBounds[uAxis] - state.sceneMinBounds[uAxis]) * 0.05f;
                float padV = (state.sceneMaxBounds[vAxis] - state.sceneMinBounds[vAxis]) * 0.05f;
                
                if (p[uAxis] >= state.sceneMinBounds[uAxis] - padU && p[uAxis] <= state.sceneMaxBounds[uAxis] + padU &&
                    p[vAxis] >= state.sceneMinBounds[vAxis] - padV && p[vAxis] <= state.sceneMaxBounds[vAxis] + padV) {
                    bestPlane = plane;
                    bestDist = t;
                    outHitPoint = p;
                }
            }
        };

        if (state.showPlaneXMin) testPlane(DraggedPlane::XMin, state.clipXMin, 0, 1, 2);
        if (state.showPlaneXMax) testPlane(DraggedPlane::XMax, state.clipXMax, 0, 1, 2);
        if (state.showPlaneYMin) testPlane(DraggedPlane::YMin, state.clipYMin, 1, 0, 2);
        if (state.showPlaneYMax) testPlane(DraggedPlane::YMax, state.clipYMax, 1, 0, 2);
        if (state.showPlaneZMin) testPlane(DraggedPlane::ZMin, state.clipZMin, 2, 0, 1);
        if (state.showPlaneZMax) testPlane(DraggedPlane::ZMax, state.clipZMax, 2, 0, 1);

        return bestPlane;
    }

    void InputController::Update(
        Window&                      window,
        Camera&                      camera,
        std::vector<std::shared_ptr<SceneModel>>& documents,
        SelectionState&              selection,
        const EngineConfig&          config,
        float                        deltaTime,
        uint32_t&                    currentLightingMode,
        bool&                        triggerFocus,
        CommandHistory&              history) 
    {
        bool isMeasuring = (selection.activeTool == InteractionTool::Measure);
        
        bool uiHovered = ImGui::GetIO().WantCaptureMouse || (!isMeasuring && ImGuizmo::IsOver());
        bool uiTyping  = ImGui::GetIO().WantTextInput;

        if (!uiTyping) {
            bool ctrlPressed = window.IsKeyPressed(GLFW_KEY_LEFT_CONTROL) || window.IsKeyPressed(GLFW_KEY_RIGHT_CONTROL);
            bool shiftPressed = window.IsKeyPressed(GLFW_KEY_LEFT_SHIFT) || window.IsKeyPressed(GLFW_KEY_RIGHT_SHIFT);

            static bool zWasDown = false;
            bool zNow = window.IsKeyPressed(GLFW_KEY_Z);
            if (zNow && !zWasDown && ctrlPressed) {
                if (shiftPressed) history.Redo();
                else history.Undo();
            }
            zWasDown = zNow;

            static bool yWasDown = false;
            bool yNow = window.IsKeyPressed(GLFW_KEY_Y);
            if (yNow && !yWasDown && ctrlPressed) {
                history.Redo();
            }
            yWasDown = yNow;

            const bool uiNow = window.IsKeyPressed(config.KeyToggleUI);
            if (uiNow && !m_uiWasDown) selection.showUI = !selection.showUI;
            m_uiWasDown = uiNow;

            // --- THE NEW F3 KEYBINDING ---
            const bool f3Now = window.IsKeyPressed(GLFW_KEY_F3);
            if (f3Now && !m_f3WasDown) selection.showSearchPanel = !selection.showSearchPanel;
            m_f3WasDown = f3Now;
            
            static bool cmdWasDown = false;
            const bool cmdNow = window.IsKeyPressed(config.KeyToggleCommandPanel);
            if (cmdNow && !m_cmdWasDown) selection.showCommandPanel = !selection.showCommandPanel;
            m_cmdWasDown = cmdNow;

            if (window.IsKeyPressed(config.KeyToolSelect)) selection.activeTool = InteractionTool::Select; 
            if (window.IsKeyPressed(config.KeyToolPan))    selection.activeTool = InteractionTool::Move; 
            if (window.IsKeyPressed(config.KeyToolOrbit))  selection.activeTool = InteractionTool::Rotate; 

            static bool mWasDown = false;
            bool mNow = window.IsKeyPressed(config.KeyToolMeasure);
            if (mNow && !mWasDown) {
                if (selection.activeTool == InteractionTool::Measure) {
                    selection.activeTool = InteractionTool::Select;
                    selection.completedMeasurements.clear();
                    selection.isMeasuringActive = false;
                } else {
                    selection.activeTool = InteractionTool::Measure;
                }
            }
            mWasDown = mNow;

            const bool tabNow = window.IsKeyPressed(config.KeyToggleNavigation);
            if (tabNow && !m_tabWasDown) {
                m_navMode = (m_navMode == NavigationMode::CAD) ? NavigationMode::Flight : NavigationMode::CAD;
                if (m_navMode == NavigationMode::Flight) window.DisableCursor();
                else window.EnableCursor();
            }
            m_tabWasDown = tabNow;

            const bool lNow = window.IsKeyPressed(config.KeyToggleLighting);
            if (lNow && !m_lWasDown) currentLightingMode = (currentLightingMode == 0) ? 1 : 0;
            m_lWasDown = lNow;
            
            selection.lightingMode = currentLightingMode; 

            const bool fNow = window.IsKeyPressed(config.KeyFocus);
            if (fNow && !m_fWasDown) triggerFocus = true;
            m_fWasDown = fNow;

            const bool hNow = window.IsKeyPressed(config.KeyHide);
            if (hNow && !m_hWasDown && !selection.objects.empty()) {
                bool allHidden = true;
                std::vector<std::string> targetGuids;
                for (auto& obj : selection.objects) {
                    targetGuids.push_back(obj.guid);
                    if (selection.hiddenObjects.count(obj.guid) == 0) allHidden = false;
                }
                history.ExecuteCommand(std::make_unique<CmdHide>(selection, targetGuids, !allHidden));
                
                if (!allHidden) {
                    selection.objects.clear();
                    selection.selectionChanged = true;
                }
            }
            m_hWasDown = hNow;

            const bool delNow = window.IsKeyPressed(config.KeyDelete);
            if (delNow && !m_delWasDown && !selection.objects.empty()) {
                std::vector<std::string> toDelete;
                for (auto& obj : selection.objects) toDelete.push_back(obj.guid);
                history.ExecuteCommand(std::make_unique<CmdDelete>(selection, toDelete));
                
                selection.objects.clear();
                selection.selectionChanged = true;
            }
            m_delWasDown = delNow;

            glm::vec3 moveDir(0.0f);
            if (window.IsKeyPressed(config.KeyForward))  moveDir.z += 1.0f;
            if (window.IsKeyPressed(config.KeyBackward)) moveDir.z -= 1.0f;
            if (window.IsKeyPressed(config.KeyRight))    moveDir.x += 1.0f;
            if (window.IsKeyPressed(config.KeyLeft))     moveDir.x -= 1.0f;
            if (window.IsKeyPressed(config.KeyUp))       moveDir.y += 1.0f;
            if (window.IsKeyPressed(config.KeyDown))     moveDir.y -= 1.0f;

            if (glm::length(moveDir) > 0.01f) {
                float speedMult = config.BaseSpeed;
                if (window.IsKeyPressed(config.KeyFast)) speedMult *= config.FlightFastMultiplier;
                else if (window.IsKeyPressed(config.KeySlow)) speedMult *= config.FlightSlowMultiplier;

                camera.ProcessKeyboard(moveDir * speedMult, deltaTime);
            }
        }

        double mx, my;
        window.GetMousePosition(mx, my);

        float rawDeltaX = static_cast<float>(mx - m_lastMouseX);
        float rawDeltaY = static_cast<float>(my - m_lastMouseY);
        m_lastMouseX = mx;
        m_lastMouseY = my;

        camera.SetZoomSpeed(config.ZoomSpeed);
        float scroll = (float)window.ConsumeScrollDelta();

        if (!uiHovered && std::abs(scroll) > 0.01f) {
            float zoomMult = 1.0f;
            if (window.IsKeyPressed(config.KeyFast)) zoomMult *= config.ZoomFastMultiplier;
            else if (window.IsKeyPressed(config.KeySlow)) zoomMult *= config.ZoomSlowMultiplier;

            camera.ProcessZoom(scroll * zoomMult);
        }

        if (m_navMode == NavigationMode::CAD) {

            if (!uiTyping && !uiHovered) {
                float keyboardOrbitSpeed = config.KeyboardOrbitSpeed * deltaTime;
                if (window.IsKeyPressed(GLFW_KEY_UP))    camera.ProcessOrbit(0.0f, keyboardOrbitSpeed);
                if (window.IsKeyPressed(GLFW_KEY_DOWN))  camera.ProcessOrbit(0.0f, -keyboardOrbitSpeed);
                if (window.IsKeyPressed(GLFW_KEY_LEFT))  camera.ProcessOrbit(-keyboardOrbitSpeed, 0.0f);
                if (window.IsKeyPressed(GLFW_KEY_RIGHT)) camera.ProcessOrbit(keyboardOrbitSpeed, 0.0f);
            }

            if (!uiHovered) {
                bool mmbPan   = window.IsMouseButtonPressed(config.CadPanButton) && !window.IsKeyPressed(config.CadOrbitModifier);
                bool mmbOrbit = window.IsMouseButtonPressed(config.CadPanButton) && window.IsKeyPressed(config.CadOrbitModifier);
                bool rmbOrbit = window.IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);

                if (mmbPan) {
                    camera.ProcessPan(rawDeltaX * config.MouseSensitivityX * config.CadPanSpeed, -rawDeltaY * config.MouseSensitivityY * config.CadPanSpeed);
                } else if (mmbOrbit || rmbOrbit) {
                    camera.ProcessOrbit(rawDeltaX * config.MouseSensitivityX * config.CadOrbitSpeed, -rawDeltaY * config.MouseSensitivityY * config.CadOrbitSpeed);
                }
            }

            bool mouseDown = window.IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
            bool mouseJustPressed = mouseDown && !m_mouseWasDown;
            Ray mouseRay = ScreenToWorldRay(mx, my, window.GetWidth(), window.GetHeight(), camera.GetViewMatrix(), camera.GetProjectionMatrix(), camera.GetPosition());
            
            bool showClips = (selection.activeTool == InteractionTool::Select || selection.activeTool == InteractionTool::Measure);

            if (selection.activeTool == InteractionTool::Measure && !uiHovered && !documents.empty()) {
                HitResult closestHit;
                closestHit.distance = 1e9f;

                float cXMin = (showClips && selection.showPlaneXMin) ? selection.clipXMin : -1e9f;
                float cXMax = (showClips && selection.showPlaneXMax) ? selection.clipXMax :  1e9f;
                float cYMin = (showClips && selection.showPlaneYMin) ? selection.clipYMin : -1e9f;
                float cYMax = (showClips && selection.showPlaneYMax) ? selection.clipYMax :  1e9f;
                float cZMin = (showClips && selection.showPlaneZMin) ? selection.clipZMin : -1e9f;
                float cZMax = (showClips && selection.showPlaneZMax) ? selection.clipZMax :  1e9f;

                for (auto& doc : documents) {
                    if (doc->IsHidden()) continue;
                    HitResult hit = Raycaster::CastRay(mouseRay, *doc, 
                                                       cXMin, cXMax, cYMin, cYMax, cZMin, cZMax, 
                                                       selection.hiddenObjects, !selection.showOpeningsAndSpaces, 
                                                       selection.explodeFactor);
                    if (hit.hit && hit.distance < closestHit.distance) {
                        closestHit = hit;
                    }
                }

                selection.isHoveringGeometry = closestHit.hit;
                
                if (closestHit.hit) {
                    bool altPressed = window.IsKeyPressed(GLFW_KEY_LEFT_ALT);
                    if (altPressed) {
                        selection.currentSnapType = SnapType::Face;
                        selection.currentSnapPoint = closestHit.hitPoint;
                    } else {
                        float threshold = closestHit.distance * 0.02f;

                        float d0 = glm::length(closestHit.hitPoint - closestHit.hitV0);
                        float d1 = glm::length(closestHit.hitPoint - closestHit.hitV1);
                        float d2 = glm::length(closestHit.hitPoint - closestHit.hitV2);

                        float minDist = std::min({d0, d1, d2});
                        if (minDist < threshold) {
                            selection.currentSnapType = SnapType::Vertex;
                            if (minDist == d0) selection.currentSnapPoint = closestHit.hitV0;
                            else if (minDist == d1) selection.currentSnapPoint = closestHit.hitV1;
                            else selection.currentSnapPoint = closestHit.hitV2;
                        } else {
                            auto closestOnLine = [](const glm::vec3& p, const glm::vec3& a, const glm::vec3& b) {
                                glm::vec3 ab = b - a;
                                float t = std::clamp(glm::dot(p - a, ab) / glm::dot(ab, ab), 0.0f, 1.0f);
                                return a + t * ab;
                            };

                            glm::vec3 e0 = closestOnLine(closestHit.hitPoint, closestHit.hitV0, closestHit.hitV1);
                            glm::vec3 e1 = closestOnLine(closestHit.hitPoint, closestHit.hitV1, closestHit.hitV2);
                            glm::vec3 e2 = closestOnLine(closestHit.hitPoint, closestHit.hitV2, closestHit.hitV0);

                            float ed0 = glm::length(closestHit.hitPoint - e0);
                            float ed1 = glm::length(closestHit.hitPoint - e1);
                            float ed2 = glm::length(closestHit.hitPoint - e2);

                            float minEd = std::min({ed0, ed1, ed2});
                            if (minEd < threshold) {
                                selection.currentSnapType = SnapType::Edge;
                                if (minEd == ed0) { selection.currentSnapPoint = e0; selection.currentSnapEdgeV0 = closestHit.hitV0; selection.currentSnapEdgeV1 = closestHit.hitV1; }
                                else if (minEd == ed1) { selection.currentSnapPoint = e1; selection.currentSnapEdgeV0 = closestHit.hitV1; selection.currentSnapEdgeV1 = closestHit.hitV2; }
                                else { selection.currentSnapPoint = e2; selection.currentSnapEdgeV0 = closestHit.hitV2; selection.currentSnapEdgeV1 = closestHit.hitV0; }
                            } else {
                                selection.currentSnapType = SnapType::Face;
                                selection.currentSnapPoint = closestHit.hitPoint;
                            }
                        }
                    }

                    if (mouseJustPressed) {
                        if (!selection.isMeasuringActive) {
                            selection.measureStartPoint = selection.currentSnapPoint;
                            selection.isMeasuringActive = true;
                        } else {
                            selection.completedMeasurements.push_back({selection.measureStartPoint, selection.currentSnapPoint});
                            selection.isMeasuringActive = false;
                        }
                    }
                } else {
                    selection.currentSnapType = SnapType::None;
                }
            } 
            else if (selection.activeTool != InteractionTool::Measure && !uiHovered && !documents.empty()) {
                
                if (mouseJustPressed) {
                    m_draggedPlane = CheckPlaneHits(mouseRay, selection, showClips, m_dragStartPoint);
                    if (m_draggedPlane != DraggedPlane::None) {
                        if (m_draggedPlane == DraggedPlane::XMin) m_dragStartClipValue = selection.clipXMin;
                        else if (m_draggedPlane == DraggedPlane::XMax) m_dragStartClipValue = selection.clipXMax;
                        else if (m_draggedPlane == DraggedPlane::YMin) m_dragStartClipValue = selection.clipYMin;
                        else if (m_draggedPlane == DraggedPlane::YMax) m_dragStartClipValue = selection.clipYMax;
                        else if (m_draggedPlane == DraggedPlane::ZMin) m_dragStartClipValue = selection.clipZMin;
                        else if (m_draggedPlane == DraggedPlane::ZMax) m_dragStartClipValue = selection.clipZMax;
                    }
                }

                if (mouseDown && m_draggedPlane != DraggedPlane::None) {
                    glm::vec3 axisDir(0.0f);
                    float* targetClip = nullptr;
                    
                    if (m_draggedPlane == DraggedPlane::XMin) { axisDir = glm::vec3(1,0,0); targetClip = &selection.clipXMin; }
                    else if (m_draggedPlane == DraggedPlane::XMax) { axisDir = glm::vec3(1,0,0); targetClip = &selection.clipXMax; }
                    else if (m_draggedPlane == DraggedPlane::YMin) { axisDir = glm::vec3(0,1,0); targetClip = &selection.clipYMin; }
                    else if (m_draggedPlane == DraggedPlane::YMax) { axisDir = glm::vec3(0,1,0); targetClip = &selection.clipYMax; }
                    else if (m_draggedPlane == DraggedPlane::ZMin) { axisDir = glm::vec3(0,0,1); targetClip = &selection.clipZMin; }
                    else if (m_draggedPlane == DraggedPlane::ZMax) { axisDir = glm::vec3(0,0,1); targetClip = &selection.clipZMax; }

                    glm::mat4 view = camera.GetViewMatrix();
                    glm::vec3 camForward = -glm::normalize(glm::vec3(view[0][2], view[1][2], view[2][2]));
                    glm::vec3 n = glm::cross(axisDir, glm::cross(axisDir, camForward));
                    if (glm::length(n) < 1e-4f) n = glm::cross(axisDir, glm::vec3(0,1,0));
                    n = glm::normalize(n);

                    float t = glm::dot(m_dragStartPoint - mouseRay.origin, n) / glm::dot(mouseRay.direction, n);
                    glm::vec3 hit = mouseRay.origin + mouseRay.direction * t;
                    
                    float delta = glm::dot(hit - m_dragStartPoint, axisDir);
                    *targetClip = m_dragStartClipValue + delta;

                    if (m_draggedPlane == DraggedPlane::XMin && selection.clipXMin > selection.clipXMax) selection.clipXMin = selection.clipXMax - 0.01f;
                    if (m_draggedPlane == DraggedPlane::XMax && selection.clipXMax < selection.clipXMin) selection.clipXMax = selection.clipXMin + 0.01f;
                    if (m_draggedPlane == DraggedPlane::YMin && selection.clipYMin > selection.clipYMax) selection.clipYMin = selection.clipYMax - 0.01f;
                    if (m_draggedPlane == DraggedPlane::YMax && selection.clipYMax < selection.clipYMin) selection.clipYMax = selection.clipYMin + 0.01f;
                    if (m_draggedPlane == DraggedPlane::ZMin && selection.clipZMin > selection.clipZMax) selection.clipZMin = selection.clipZMax - 0.01f;
                    if (m_draggedPlane == DraggedPlane::ZMax && selection.clipZMax < selection.clipZMin) selection.clipZMax = selection.clipZMin + 0.01f;

                } 
                else if (mouseJustPressed && m_draggedPlane == DraggedPlane::None) {
                    HandleMousePicking(window, camera, documents, selection, config, history);
                }
            }

            if (!mouseDown) {
                m_draggedPlane = DraggedPlane::None;
            }

            m_mouseWasDown = mouseDown;

        } else {
            camera.ProcessMouseMovement(rawDeltaX * config.MouseSensitivityX * config.FlightMouseSpeed, -rawDeltaY * config.MouseSensitivityY * config.FlightMouseSpeed);
        }
    }

    void InputController::HandleMousePicking(Window& window, Camera& camera, std::vector<std::shared_ptr<SceneModel>>& documents, SelectionState& selection, const EngineConfig& config, CommandHistory& history) {
        double mx, my;
        window.GetMousePosition(mx, my);
        Ray ray = ScreenToWorldRay(mx, my, window.GetWidth(), window.GetHeight(), camera.GetViewMatrix(), camera.GetProjectionMatrix(), camera.GetPosition());

        HitResult closestHit;
        closestHit.distance = 1e9f;
        std::shared_ptr<SceneModel> hitDoc = nullptr;

        bool showClips = (selection.activeTool == InteractionTool::Select);

        float cXMin = (showClips && selection.showPlaneXMin) ? selection.clipXMin : -1e9f;
        float cXMax = (showClips && selection.showPlaneXMax) ? selection.clipXMax :  1e9f;
        float cYMin = (showClips && selection.showPlaneYMin) ? selection.clipYMin : -1e9f;
        float cYMax = (showClips && selection.showPlaneYMax) ? selection.clipYMax :  1e9f;
        float cZMin = (showClips && selection.showPlaneZMin) ? selection.clipZMin : -1e9f;
        float cZMax = (showClips && selection.showPlaneZMax) ? selection.clipZMax :  1e9f;

        for (auto& doc : documents) {
            if (doc->IsHidden()) continue;
            
            HitResult hit = Raycaster::CastRay(ray, *doc, 
                                               cXMin, cXMax, cYMin, cYMax, cZMin, cZMax, 
                                               selection.hiddenObjects, !selection.showOpeningsAndSpaces, 
                                               selection.explodeFactor);
                                               
            if (hit.hit && hit.distance < closestHit.distance) {
                closestHit = hit;
                hitDoc = doc;
            }
        }

        bool isCtrlPressed = window.IsKeyPressed(GLFW_KEY_LEFT_CONTROL) || window.IsKeyPressed(GLFW_KEY_RIGHT_CONTROL);

        if (closestHit.hit && hitDoc) {
            if (!isCtrlPressed) selection.objects.clear();

            std::string baseClickGuid = closestHit.hitGuid.length() >= 22 ? closestHit.hitGuid.substr(0, 22) : closestHit.hitGuid;

            if (selection.selectAssemblies) {
                std::string rootGuid = baseClickGuid;
                
                while (true) {
                    std::string p = hitDoc->GetParent(rootGuid);
                    if (p.empty()) break;
                    
                    try {
                        IfcUtil::IfcBaseClass* parentObj = hitDoc->GetDatabase()->instance_by_guid(p);
                        if (parentObj) {
                            std::string pType = parentObj->declaration().name();
                            if (pType == "IfcBuildingStorey" || pType == "IfcBuilding" || 
                                pType == "IfcSite" || pType == "IfcProject" || pType == "IfcSpace") {
                                break;
                            }
                        }
                    } catch(...) {}

                    rootGuid = p;
                }

                bool toggleOff = false;
                if (isCtrlPressed) {
                    auto it = std::find_if(selection.objects.begin(), selection.objects.end(),
                                           [&](const SelectedObject& o) { return o.guid == closestHit.hitGuid; });
                    if (it != selection.objects.end()) toggleOff = true;
                }

                std::vector<std::string> familyBaseGuids;
                std::vector<std::string> stack = { rootGuid };
                while (!stack.empty()) {
                    std::string curr = stack.back();
                    stack.pop_back();
                    familyBaseGuids.push_back(curr);
                    std::vector<std::string> kids = hitDoc->GetChildren(curr);
                    stack.insert(stack.end(), kids.begin(), kids.end());
                }

                if (toggleOff) {
                    selection.objects.erase(
                        std::remove_if(selection.objects.begin(), selection.objects.end(),
                                       [&](const SelectedObject& o) {
                                           std::string bg = o.guid.length() >= 22 ? o.guid.substr(0, 22) : o.guid;
                                           return std::find(familyBaseGuids.begin(), familyBaseGuids.end(), bg) != familyBaseGuids.end();
                                       }
                        ), selection.objects.end()
                    );
                } else {
                    for (const auto& sub : hitDoc->GetGeometry().subMeshes) {
                        std::string bg = sub.guid.length() >= 22 ? sub.guid.substr(0, 22) : sub.guid;
                        if (std::find(familyBaseGuids.begin(), familyBaseGuids.end(), bg) != familyBaseGuids.end()) {
                            auto it = std::find_if(selection.objects.begin(), selection.objects.end(),
                                                   [&](const SelectedObject& o) { return o.guid == sub.guid; });
                            if (it == selection.objects.end()) {
                                SelectedObject so;
                                so.guid = sub.guid;
                                so.type = sub.type;
                                so.startIndex = sub.globalStartIndex;
                                so.indexCount = sub.indexCount;
                                so.properties = hitDoc->GetElementProperties(bg);
                                selection.objects.push_back(so);
                            }
                        }
                    }
                }
            } else {
                auto it = std::find_if(selection.objects.begin(), selection.objects.end(),
                                       [&](const SelectedObject& o) { return o.guid == closestHit.hitGuid; });

                if (it != selection.objects.end()) {
                    if (isCtrlPressed) selection.objects.erase(it);
                } else {
                    uint32_t globalStart = 0;
                    for (const auto& sub : hitDoc->GetGeometry().subMeshes) {
                        if (sub.guid == closestHit.hitGuid) {
                            globalStart = sub.globalStartIndex;
                            break;
                        }
                    }

                    SelectedObject so;
                    so.guid = closestHit.hitGuid;
                    so.type = closestHit.hitType;
                    so.startIndex = globalStart;
                    so.indexCount = closestHit.hitIndexCount;
                    so.properties = hitDoc->GetElementProperties(baseClickGuid);
                    selection.objects.push_back(so);
                }
            }

            selection.selectionChanged = true;
        }
    }

    Ray InputController::ScreenToWorldRay(double mouseX, double mouseY, int screenW, int screenH, const glm::mat4& view, const glm::mat4& proj, const glm::vec3& camPos) {
        float x = (2.0f * (float)mouseX) / (float)screenW - 1.0f;
        float y = 1.0f - (2.0f * (float)mouseY) / (float)screenH;
        glm::vec4 rayClip(x, y, -1.0f, 1.0f);
        glm::vec4 rayEye = glm::inverse(proj) * rayClip;
        rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);
        glm::vec3 rayWorld = glm::normalize(glm::vec3(glm::inverse(view) * rayEye));
        return { {camPos.x, camPos.y, camPos.z}, {rayWorld.x, rayWorld.y, rayWorld.z} };
    }

} // namespace BimCore