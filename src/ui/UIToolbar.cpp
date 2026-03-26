// =============================================================================
// BimCore/apps/editor/ui/UIToolbar.cpp
// =============================================================================
#include "UIToolbar.h"
#include "UIMainPanel.h" // For DrawResetModal
#include <imgui.h>

#define ICON_FA_FOLDER_OPEN   "\xef\x81\xbc"
#define ICON_FA_SAVE          "\xef\x83\x87"
#define ICON_FA_FILE_IMPORT   "\xef\x95\xaf" 
#define ICON_FA_FILE_EXPORT   "\xef\x95\xae" 
#define ICON_FA_MOUSE_POINTER "\xef\x89\x85"
#define ICON_FA_ARROWS_ALT    "\xef\x82\xb2"
#define ICON_FA_SYNC          "\xef\x80\xa1"
#define ICON_FA_TIMES_CIRCLE  "\xef\x81\x97"
#define ICON_FA_EYE           "\xef\x81\xae"
#define ICON_FA_HISTORY       "\xef\x87\xba"
#define ICON_FA_RULER         "\xef\x95\x85"
#define ICON_FA_DOOR_OPEN     "\xef\x94\xa2"
#define ICON_FA_CUBE          "\xef\x86\xb2"
#define ICON_FA_OBJECT_GROUP  "\xef\x89\x87"
#define ICON_FA_VECTOR_SQUARE "\xef\x97\x8b"

namespace BimCore {

    void UIToolbar::Render(SelectionState& state, std::vector<std::shared_ptr<SceneModel>>& documents, float configMaxExplode, CommandHistory& history, bool& triggerRebuild) {
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.55f));
        float bigBtnDim = ImGui::GetFrameHeight() * 1.5f;
        ImVec2 bigBtnSize(bigBtnDim, bigBtnDim);

        if (ImGui::Button(ICON_FA_FOLDER_OPEN, bigBtnSize)) state.triggerLoad = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Open Native IFC");
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_SAVE, bigBtnSize)) state.triggerSave = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save Native IFC");
        ImGui::SameLine();

        if (ImGui::Button(ICON_FA_FILE_IMPORT, bigBtnSize)) ImGui::OpenPopup("ImportMenu");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Import External Data");
        if (ImGui::BeginPopup("ImportMenu")) {
            ImGui::TextDisabled("Import Format");
            ImGui::Separator();
            if (ImGui::MenuItem("Selection (.csv)")) state.triggerImport = 1;
            if (ImGui::MenuItem("Issues/Clashes (.bcf)")) state.triggerImport = 2;
            if (ImGui::MenuItem("3D Geometry (.gltf / .glb / .stl)")) state.triggerImport = 3;
            ImGui::EndPopup();
        }
        ImGui::SameLine();

        if (ImGui::Button(ICON_FA_FILE_EXPORT, bigBtnSize)) ImGui::OpenPopup("ExportMenu");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Export Geometry");
        if (ImGui::BeginPopup("ExportMenu")) {
            ImGui::TextDisabled("Export Format");
            ImGui::Separator();
            if (ImGui::MenuItem("3D Geometry (.gltf / .glb)")) state.triggerExport = 1;
            if (ImGui::MenuItem("Raw Triangles (.stl)")) state.triggerExport = 2;
            ImGui::EndPopup();
        }

        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float rightButtonGroupWidth = (bigBtnDim * 4.0f) + (spacing * 3.0f);
        float cursorX = ImGui::GetWindowContentRegionMax().x - rightButtonGroupWidth;

        if (cursorX > ImGui::GetCursorPosX()) ImGui::SameLine(cursorX);
        else ImGui::SameLine();

        bool isExploded = state.explodeFactor > 0.01f;
        
        auto drawToolBtn = [&](InteractionTool tool, const char* icon, const char* id, bool disabled = false) {
            bool isToolActive = (state.activeTool == tool);
            
            if (disabled) {
                ImGui::BeginDisabled();
                if (isToolActive) { 
                    state.activeTool = InteractionTool::Select;
                    isToolActive = false;
                }
            }
            
            if (isToolActive) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            
            if (ImGui::Button(icon, bigBtnSize)) {
                state.activeTool = tool;
            }
            
            if (isToolActive) ImGui::PopStyleColor();
            if (disabled) ImGui::EndDisabled();
            
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip(disabled ? "Disabled while model is exploded" : "%s", id);
            }
        };

        drawToolBtn(InteractionTool::Select, ICON_FA_MOUSE_POINTER, "Select (1)"); ImGui::SameLine();
        drawToolBtn(InteractionTool::Move,   ICON_FA_ARROWS_ALT,    "Move (2)", isExploded);   ImGui::SameLine();
        drawToolBtn(InteractionTool::Rotate, ICON_FA_SYNC,          "Rotate (3)", isExploded); ImGui::SameLine();

        bool isMeasActive = state.measureToolActive;
        if (isMeasActive) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(ICON_FA_RULER, bigBtnSize)) {
            state.measureToolActive = !state.measureToolActive;
            if (!state.measureToolActive) {
                state.completedMeasurements.clear();
                state.isMeasuringActive = false;
            }
        }
        if (isMeasActive) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle Measure Tool (M)");

        ImGui::Spacing();
        if (ImGui::Button(ICON_FA_TIMES_CIRCLE, bigBtnSize)) {
            state.objects.clear();
            state.selectionChanged = true;
            state.completedMeasurements.clear();
            state.isMeasuringActive = false;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear selected & measurements");
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_EYE, bigBtnSize)) { 
            state.hiddenObjects.clear(); 
            for(auto& d : documents) d->SetHidden(false); 
            state.hiddenStateChanged = true; 
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show all");
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_HISTORY, bigBtnSize)) { ImGui::OpenPopup("Reset Model"); }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset");

        cursorX = ImGui::GetWindowContentRegionMax().x - (bigBtnDim * 5.0f) - (spacing * 4.0f);
        if (cursorX > ImGui::GetCursorPosX()) ImGui::SameLine(cursorX);
        else ImGui::SameLine();

        bool isShowOps = state.showOpeningsAndSpaces;
        if (isShowOps) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(ICON_FA_DOOR_OPEN, bigBtnSize)) {
            state.showOpeningsAndSpaces = !isShowOps;
            state.hiddenStateChanged = true; 
        }
        if (isShowOps) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle Openings and Spaces");

        ImGui::SameLine();

        bool isStyleSolid = (state.style == 1);
        if (isStyleSolid) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(ICON_FA_CUBE, bigBtnSize)) state.style = isStyleSolid ? 0 : 1;
        if (isStyleSolid) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle Selection Style (Solid/Outline)");

        ImGui::SameLine();

        bool isSelAss = state.selectAssemblies;
        if (isSelAss) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(ICON_FA_OBJECT_GROUP, bigBtnSize)) state.selectAssemblies = !isSelAss;
        if (isSelAss) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle Selection Mode (Parts vs Full Assemblies)");

        ImGui::SameLine();

        bool isShowBBox = state.showBoundingBox;
        if (isShowBBox) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(ICON_FA_VECTOR_SQUARE, bigBtnSize)) state.showBoundingBox = !isShowBBox;
        if (isShowBBox) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle Selection Bounding Box");

        ImGui::SameLine();

        ImVec4 imColor(state.color.x, state.color.y, state.color.z, state.color.w);
        if (ImGui::ColorButton("##selcolorbtn", imColor, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoAlpha, bigBtnSize)) {
            ImGui::OpenPopup("ColorPickerPopup");
        }
        if (ImGui::BeginPopup("ColorPickerPopup")) {
            ImGui::ColorPicker4("Selection Color", &state.color.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
            ImGui::EndPopup();
        }

        ImGui::PopStyleVar();

        UIMainPanel::DrawResetModal(state, documents, triggerRebuild, history);

        ImGui::Separator();

        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.30f, 0.35f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.30f, 0.35f, 0.40f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.35f, 0.40f, 0.45f, 1.0f));
        bool isExplodeOpen = ImGui::CollapsingHeader("Explode");
        ImGui::PopStyleColor(3);

        if (isExplodeOpen) {
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SliderFloat("##explode", &state.explodeFactor, 0.0f, configMaxExplode, "%.2fx");
            if (ImGui::IsItemActive()) state.updateGeometry = true;
        }

        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.30f, 0.35f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.30f, 0.35f, 0.40f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.35f, 0.40f, 0.45f, 1.0f));
        bool isClippingOpen = ImGui::CollapsingHeader("Clipping Planes");
        ImGui::PopStyleColor(3);

        if (isClippingOpen) {
            float bMinX = state.sceneMinBounds[0]; float bMaxX = state.sceneMaxBounds[0];
            float bMinY = state.sceneMinBounds[1]; float bMaxY = state.sceneMaxBounds[1];
            float bMinZ = state.sceneMinBounds[2]; float bMaxZ = state.sceneMaxBounds[2];

            auto drawClipRow = [](const char* axis, bool& showMin, float& valMin, bool& showMax, float& valMax, float minB, float maxB, float* col) {
                ImGui::PushID(axis);
                ImGui::Text("%s Axis", axis);
                ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::GetFrameHeight());
                ImGui::ColorEdit3("##col", col, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoAlpha);
                ImGui::Checkbox("Min", &showMin); ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::DragFloat("##vmin", &valMin, 0.05f, minB, valMax, "%.2f");
                ImGui::Checkbox("Max", &showMax); ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::DragFloat("##vmax", &valMax, 0.05f, valMin, maxB, "%.2f");
                ImGui::Separator();
                ImGui::PopID();
            };

            drawClipRow("X", state.showPlaneXMin, state.clipXMin, state.showPlaneXMax, state.clipXMax, bMinX, bMaxX, state.planeColorX);
            drawClipRow("Y", state.showPlaneYMin, state.clipYMin, state.showPlaneYMax, state.clipYMax, bMinY, bMaxY, state.planeColorY);
            drawClipRow("Z", state.showPlaneZMin, state.clipZMin, state.showPlaneZMax, state.clipZMax, bMinZ, bMaxZ, state.planeColorZ);
        }
        ImGui::Separator();
    }
} // namespace BimCore