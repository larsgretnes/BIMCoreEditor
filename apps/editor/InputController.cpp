// =============================================================================
// BimCore/apps/editor/InputController.cpp
// =============================================================================
#include "InputController.h"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include "Core.h"

namespace BimCore {

    bool InputController::IsFlightMode() const {
        return m_navMode == NavigationMode::Flight;
    }

    void InputController::Update(
        Window&                      window,
        Camera&                      camera,
        std::shared_ptr<BimDocument> document,
        SelectionState&              selection,
        const EngineConfig&          config,
        float                        deltaTime,
        uint32_t&                    currentLightingMode,
        bool&                        triggerFocus)
    {
        bool uiHovered = ImGui::GetIO().WantCaptureMouse;
        bool uiTyping  = ImGui::GetIO().WantTextInput;

        if (!uiTyping) {
            const bool uiNow = window.IsKeyPressed(config.KeyToggleUI);
            if (uiNow && !m_uiWasDown) selection.showUI = !selection.showUI;
            m_uiWasDown = uiNow;

            if (window.IsKeyPressed(config.KeyToolSelect)) selection.activeTool = InteractionTool::Select;
            if (window.IsKeyPressed(config.KeyToolPan))    selection.activeTool = InteractionTool::Pan;
            if (window.IsKeyPressed(config.KeyToolOrbit))  selection.activeTool = InteractionTool::Orbit;

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

            const bool fNow = window.IsKeyPressed(config.KeyFocus);
            if (fNow && !m_fWasDown) triggerFocus = true;
            m_fWasDown = fNow;

            const bool hNow = window.IsKeyPressed(config.KeyHide);
            if (hNow && !m_hWasDown && !selection.objects.empty()) {
                bool allHidden = true;
                for (auto& obj : selection.objects) {
                    if (selection.hiddenObjects.count(obj.guid) == 0) allHidden = false;
                }
                for (auto& obj : selection.objects) {
                    if (allHidden) selection.hiddenObjects.erase(obj.guid);
                    else selection.hiddenObjects.insert(obj.guid);
                }
                if (!allHidden) selection.objects.clear();
                selection.hiddenStateChanged = true;
            }
            m_hWasDown = hNow;

            const bool delNow = window.IsKeyPressed(config.KeyDelete);
            if (delNow && !m_delWasDown && !selection.objects.empty()) {
                for (auto& obj : selection.objects) {
                    selection.deletedObjects.insert(obj.guid);
                    selection.hiddenObjects.insert(obj.guid);
                }
                selection.objects.clear();
                selection.hiddenStateChanged = true;
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
                float speedMult = config.BaseSpeed * (window.IsKeyPressed(config.KeySprint) ? config.SprintMultiplier : 1.0f);
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
            float zoomMult = window.IsKeyPressed(config.KeySprint) ? config.ZoomSlowMultiplier : 1.0f;
            camera.ProcessZoom(scroll * zoomMult);
        }

        if (m_navMode == NavigationMode::CAD) {

            if (!uiTyping && !uiHovered) {
                // --- FIXED: Increased arrow key steering speed by 5x (from 100 to 500) ---
                float keyboardOrbitSpeed = 500.0f * deltaTime;
                if (window.IsKeyPressed(GLFW_KEY_UP))    camera.ProcessOrbit(0.0f, keyboardOrbitSpeed);
                if (window.IsKeyPressed(GLFW_KEY_DOWN))  camera.ProcessOrbit(0.0f, -keyboardOrbitSpeed);
                if (window.IsKeyPressed(GLFW_KEY_LEFT))  camera.ProcessOrbit(-keyboardOrbitSpeed, 0.0f);
                if (window.IsKeyPressed(GLFW_KEY_RIGHT)) camera.ProcessOrbit(keyboardOrbitSpeed, 0.0f);
            }

            if (!uiHovered) {
                bool leftClickPan   = (selection.activeTool == InteractionTool::Pan) && window.IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
                bool leftClickOrbit = (selection.activeTool == InteractionTool::Orbit) && window.IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
                bool mmbPan   = window.IsMouseButtonPressed(config.CadPanButton) && !window.IsKeyPressed(config.CadOrbitModifier);
                bool mmbOrbit = window.IsMouseButtonPressed(config.CadPanButton) && window.IsKeyPressed(config.CadOrbitModifier);
                bool rmbOrbit = window.IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);

                if (leftClickPan || mmbPan) {
                    camera.ProcessPan(rawDeltaX * config.MouseSensitivityX * 250.0f, -rawDeltaY * config.MouseSensitivityY * 250.0f);
                } else if (leftClickOrbit || mmbOrbit || rmbOrbit) {
                    camera.ProcessOrbit(rawDeltaX * config.MouseSensitivityX * 250.0f, -rawDeltaY * config.MouseSensitivityY * 250.0f);
                }
            }

            if (selection.activeTool == InteractionTool::Select && !uiHovered) {
                HandleMousePicking(window, camera, document, selection, config);
            }

        } else {
            camera.ProcessMouseMovement(rawDeltaX * config.MouseSensitivityX * 5.0f, -rawDeltaY * config.MouseSensitivityY * 5.0f);
        }
    }

    void InputController::HandleMousePicking(Window& window, Camera& camera, std::shared_ptr<BimDocument> document, SelectionState& selection, const EngineConfig& config) {
        bool mouseDown = window.IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
        if (mouseDown && !m_mouseWasDown) {
            double mx, my;
            window.GetMousePosition(mx, my);
            Ray ray = ScreenToWorldRay(mx, my, window.GetWidth(), window.GetHeight(), camera.GetViewMatrix(), camera.GetProjectionMatrix(), camera.GetPosition());

            float cXMin = selection.clipXMin;
            float cXMax = selection.clipXMax;
            float cYMin = selection.clipYMin;
            float cYMax = selection.clipYMax;
            float cZMin = selection.clipZMin;
            float cZMax = selection.clipZMax;

            HitResult hit = Raycaster::CastRay(ray, document->GetGeometry(), cXMin, cXMax, cYMin, cYMax, cZMin, cZMax, selection.hiddenObjects);

            bool isCtrlPressed = window.IsKeyPressed(GLFW_KEY_LEFT_CONTROL) || window.IsKeyPressed(GLFW_KEY_RIGHT_CONTROL);

            if (hit.hit) {
                if (!isCtrlPressed) selection.objects.clear();

                auto it = std::find_if(selection.objects.begin(), selection.objects.end(),
                                       [&](const SelectedObject& o) { return o.guid == hit.hitGuid; });

                if (it != selection.objects.end()) {
                    if (isCtrlPressed) selection.objects.erase(it);
                } else {
                    SelectedObject so;
                    so.guid = hit.hitGuid;
                    so.type = hit.hitType;
                    so.startIndex = hit.hitStartIndex;
                    so.indexCount = hit.hitIndexCount;
                    so.properties = document->GetElementProperties(so.guid);
                    selection.objects.push_back(so);
                }
                selection.selectionChanged = true;
            } else {
                if (!isCtrlPressed) {
                    selection.objects.clear();
                    selection.selectionChanged = true;
                }
            }
        }
        m_mouseWasDown = mouseDown;
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
