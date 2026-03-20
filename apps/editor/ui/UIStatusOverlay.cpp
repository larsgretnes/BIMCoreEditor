// =============================================================================
// BimCore/apps/editor/ui/UIStatusOverlay.cpp
// =============================================================================
#include "UIStatusOverlay.h"
#include <imgui.h>
#include <imgui_internal.h>

// FontAwesome Icons used in overlays
#define ICON_FA_ARROWS_ALT    "\xef\x82\xb2"
#define ICON_FA_MOUSE_POINTER "\xef\x89\x85"
#define ICON_FA_EYE           "\xef\x81\xae"
#define ICON_FA_TRASH         "\xef\x80\x8d"

namespace BimCore {

    void UIStatusOverlay::RenderFlyMode(bool isFlightMode) {
        if (!isFlightMode) return;

        ImGuiIO& io = ImGui::GetIO();
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);

        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + 20.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
        ImGui::SetNextWindowBgAlpha(0.65f);
        ImGui::Begin("FlyModeOverlay", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove);

        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), ICON_FA_ARROWS_ALT "  FLY MODE ACTIVE");
        ImGui::Text("Press F1 to unlock cursor");
        ImGui::End();
    }

    void UIStatusOverlay::RenderStatusPanel(SelectionState& state, std::shared_ptr<BimDocument> document) {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float statsPanelHeight = 150.0f;
        const float mainPanelHeight = viewport->WorkSize.y - statsPanelHeight;

        ImGuiContext* g = ImGui::GetCurrentContext();
        ImGuiWindow* mainWin = ImGui::FindWindowByName("Main Menu");
        float currentLeftWidth = mainWin ? mainWin->Size.x : 400.0f;

        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + mainPanelHeight), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(currentLeftWidth, statsPanelHeight), ImGuiCond_Always);
        ImGui::Begin("Status", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

        bool isActivelyLoading = state.loadState && !state.loadState->isLoaded.load() && state.loadState->progress.load() > 0.0f;
        if (isActivelyLoading) {
            ImGui::Text("%s", state.loadState->GetStatus().c_str());
            ImGui::ProgressBar(state.loadState->progress.load());
            ImGui::Separator();
        }

        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        if (document) {
            ImGui::Text("Elements: %zu", document->GetGeometry().subMeshes.size());
            ImGui::Text("Vertices: %zu", document->GetGeometry().vertices.size());
            ImGui::Text("Selected: %zu", state.objects.size());
        }
        ImGui::End();
    }

    void UIStatusOverlay::RenderContextMenu(SelectionState& state, bool& triggerFocus) {
        bool isHoveringUI = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) || ImGui::IsAnyItemHovered();

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) &&
            (ImGui::GetIO().MouseDragMaxDistanceSqr[ImGuiMouseButton_Right] < 25.0f) &&
            !isHoveringUI)
        {
            ImGui::OpenPopup("##3DContextMenu");
        }

        if (ImGui::BeginPopup("##3DContextMenu")) {
            if (!state.objects.empty()) {
                ImGui::TextDisabled("%zu Elements Selected", state.objects.size());
                ImGui::Separator();

                if (ImGui::MenuItem(ICON_FA_MOUSE_POINTER "  Focus Selection")) { triggerFocus = true; }

                bool anyVisible = false;
                for (const auto& obj : state.objects) {
                    if (state.hiddenObjects.count(obj.guid) == 0) { anyVisible = true; break; }
                }

                if (ImGui::MenuItem(anyVisible ? ICON_FA_EYE "  Hide Selection" : ICON_FA_EYE "  Show Selection")) {
                    for (const auto& obj : state.objects) {
                        if (anyVisible) state.hiddenObjects.insert(obj.guid);
                        else state.hiddenObjects.erase(obj.guid);
                    }
                    state.hiddenStateChanged = true;
                }

                if (ImGui::MenuItem(ICON_FA_TRASH "  Delete Selection")) {
                    for (const auto& obj : state.objects) {
                        state.deletedObjects.insert(obj.guid);
                        state.hiddenObjects.insert(obj.guid);
                    }
                    state.objects.clear();
                    state.hiddenStateChanged = true;
                }
            } else {
                ImGui::TextDisabled("Select elements to view actions.");
            }
            ImGui::EndPopup();
        }
    }

} // namespace BimCore
