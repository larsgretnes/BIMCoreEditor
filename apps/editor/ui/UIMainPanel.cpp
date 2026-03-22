// =============================================================================
// BimCore/apps/editor/ui/UIMainPanel.cpp
// =============================================================================
#include "UIMainPanel.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <thread>
#include <cctype>
#include <fstream>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include "platform/portable-file-dialogs.h"

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
#define ICON_FA_SEARCH        "\xef\x80\x82"
#define ICON_FA_DOWNLOAD      "\xef\x80\x99"
#define ICON_FA_UNDO          "\xef\x80\x9e"
#define ICON_FA_CUBE          "\xef\x86\xb2"
#define ICON_FA_VECTOR_SQUARE "\xef\x97\x8b"
#define ICON_FA_DOOR_OPEN     "\xef\x94\xa2"
#define ICON_FA_OBJECT_GROUP  "\xef\x89\x87"
#define ICON_FA_RULER         "\xef\x95\x85"

namespace BimCore {

    static bool icontains(const std::string& str, const std::string& query) {
        if (query.empty()) return true;
        auto it = std::search(str.begin(), str.end(), query.begin(), query.end(),
                              [](unsigned char ch1, unsigned char ch2) { return std::tolower(ch1) == std::tolower(ch2); });
        return it != str.end();
    }

    void UIMainPanel::Render(SelectionState& state, std::vector<std::shared_ptr<BimDocument>>& documents, float configMaxExplode, bool& triggerFocus, bool& triggerRebuild) {

        if (state.measureToolActive) {
            ImDrawList* drawList = ImGui::GetBackgroundDrawList();
            ImU32 colorMeas     = IM_COL32(255, 165, 0, 255);
            ImU32 colorSnapVert = IM_COL32(0, 255, 255, 255);
            ImU32 colorSnapEdge = IM_COL32(255, 0, 255, 255);
            ImU32 colorSnapFace = IM_COL32(255, 255, 0, 255);

            auto drawText = [&](const ImVec2& p1, const ImVec2& p2, const char* text) {
                if (text[0] == '\0') return;
                ImVec2 mid((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);
                ImVec2 tsz = ImGui::CalcTextSize(text);
                drawList->AddRectFilled(ImVec2(mid.x - 4, mid.y - 4), ImVec2(mid.x + tsz.x + 4, mid.y + tsz.y + 4), IM_COL32(30, 30, 30, 220), 4.0f);
                drawList->AddText(mid, IM_COL32(255, 255, 255, 255), text);
            };

            for (const auto& m : state.renderMeasurements) {
                ImVec2 p1(m.p1[0], m.p1[1]);
                ImVec2 p2(m.p2[0], m.p2[1]);
                drawList->AddLine(p1, p2, colorMeas, 3.0f);
                drawList->AddCircleFilled(p1, 5.0f, colorMeas);
                drawList->AddCircleFilled(p2, 5.0f, colorMeas);
                drawText(p1, p2, m.text);
            }

            if (state.drawActiveLine) {
                ImVec2 p1(state.renderActiveLine.p1[0], state.renderActiveLine.p1[1]);
                drawList->AddCircleFilled(p1, 5.0f, colorMeas);
                if (state.renderActiveLine.text[0] != '\0') {
                    ImVec2 p2(state.renderActiveLine.p2[0], state.renderActiveLine.p2[1]);
                    drawList->AddLine(p1, p2, colorMeas, 2.0f);
                    drawText(p1, p2, state.renderActiveLine.text);
                }
            }

            if (state.renderSnap.draw) {
                ImVec2 sp(state.renderSnap.p[0], state.renderSnap.p[1]);
                if (state.renderSnap.type == SnapType::Vertex) {
                    drawList->AddCircle(sp, 10.0f, colorSnapVert, 0, 2.0f);
                    drawList->AddCircleFilled(sp, 4.0f, colorSnapVert);
                } else if (state.renderSnap.type == SnapType::Edge) {
                    ImVec2 e0(state.renderSnap.e0[0], state.renderSnap.e0[1]);
                    ImVec2 e1(state.renderSnap.e1[0], state.renderSnap.e1[1]);
                    drawList->AddLine(e0, e1, colorSnapEdge, 4.0f);
                    drawList->AddCircleFilled(sp, 4.0f, colorSnapEdge);
                } else if (state.renderSnap.type == SnapType::Face) {
                    drawList->AddCircleFilled(sp, 4.0f, colorSnapFace);
                }
            }

            ImVec2 textPos(ImGui::GetMainViewport()->WorkSize.x * 0.5f, 40.0f);
            const char* inst = "Measure Tool: Click to start/end. Hold L-ALT to disable snapping. Turn tool off to clear.";
            ImVec2 tsz = ImGui::CalcTextSize(inst);
            drawList->AddRectFilled(ImVec2(textPos.x - tsz.x*0.5f - 10, textPos.y - 10), ImVec2(textPos.x + tsz.x*0.5f + 10, textPos.y + tsz.y + 10), IM_COL32(0,0,0,200), 5.0f);
            drawList->AddText(ImVec2(textPos.x - tsz.x*0.5f, textPos.y), IM_COL32(255,255,255,255), inst);
        }

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float statsPanelHeight = 75.0f;
        const float mainPanelHeight = viewport->WorkSize.y - statsPanelHeight;

        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y), ImGuiCond_Always);
        ImGui::SetNextWindowSizeConstraints(ImVec2(300, mainPanelHeight), ImVec2(viewport->WorkSize.x / 2.0f, mainPanelHeight));
        ImGui::SetNextWindowSize(ImVec2(400.0f, mainPanelHeight), ImGuiCond_FirstUseEver);

        ImGui::Begin("Main Menu", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

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
            if (ImGui::MenuItem("3D Geometry (.gltf / .glb)")) state.triggerImport = 3;
            ImGui::EndPopup();
        }
        ImGui::SameLine();

        if (ImGui::Button(ICON_FA_FILE_EXPORT, bigBtnSize)) ImGui::OpenPopup("ExportMenu");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Export Geometry");
        if (ImGui::BeginPopup("ExportMenu")) {
            ImGui::TextDisabled("Export Format");
            ImGui::Separator();
            if (ImGui::MenuItem("3D Geometry (.gltf / .glb)")) state.triggerExport = 1;
            ImGui::EndPopup();
        }

        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float rightButtonGroupWidth = (bigBtnDim * 4.0f) + (spacing * 3.0f);
        float cursorX = ImGui::GetWindowContentRegionMax().x - rightButtonGroupWidth;

        if (cursorX > ImGui::GetCursorPosX()) ImGui::SameLine(cursorX);
        else ImGui::SameLine();

        auto drawToolBtn = [&](InteractionTool tool, const char* icon, const char* id) {
            bool isToolActive = (state.activeTool == tool);
            if (isToolActive) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button(icon, bigBtnSize)) {
                state.activeTool = tool;
            }
            if (isToolActive) ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", id);
        };

            drawToolBtn(InteractionTool::Select, ICON_FA_MOUSE_POINTER, "Select (1)"); ImGui::SameLine();
            drawToolBtn(InteractionTool::Pan, ICON_FA_ARROWS_ALT, "Pan (2)"); ImGui::SameLine();
            drawToolBtn(InteractionTool::Orbit, ICON_FA_SYNC, "Orbit (3)"); ImGui::SameLine();

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
            for(auto& d : documents) d->SetHidden(false); // Make sure parent models are shown too
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
            state.groupsBuilt = false;
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
        DrawResetModal(state, documents, triggerRebuild);
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
            float bMinX = -100.0f, bMaxX = 100.0f;
            float bMinY = -100.0f, bMaxY = 100.0f;
            float bMinZ = -100.0f, bMaxZ = 100.0f;

            if (!documents.empty()) {
                bMinX = kFloatMax; bMaxX = kFloatMin;
                bMinY = kFloatMax; bMaxY = kFloatMin;
                bMinZ = kFloatMax; bMaxZ = kFloatMin;
                for (auto& doc : documents) {
                    auto& geom = doc->GetGeometry();
                    if (geom.minBounds[0] < bMinX) bMinX = geom.minBounds[0];
                    if (geom.maxBounds[0] > bMaxX) bMaxX = geom.maxBounds[0];
                    if (geom.minBounds[1] < bMinY) bMinY = geom.minBounds[1];
                    if (geom.maxBounds[1] > bMaxY) bMaxY = geom.maxBounds[1];
                    if (geom.minBounds[2] < bMinZ) bMinZ = geom.minBounds[2];
                    if (geom.maxBounds[2] > bMaxZ) bMaxZ = geom.maxBounds[2];
                }

                float expand = 0.0f;
                if (state.explodeFactor > 0.01f) {
                    float extX = bMaxX - bMinX;
                    float extY = bMaxY - bMinY;
                    float extZ = bMaxZ - bMinZ;
                    float maxRadius = std::sqrt(extX*extX + extY*extY + extZ*extZ) * 0.5f;
                    expand = maxRadius * state.explodeFactor;
                }

                bMinX -= (0.1f + expand); bMaxX += (0.1f + expand);
                bMinY -= (0.1f + expand); bMaxY += (0.1f + expand);
                bMinZ -= (0.1f + expand); bMaxZ += (0.1f + expand);
            }

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

        if (documents.empty()) {
            ImGui::TextDisabled("No models loaded.");
            ImGui::End();
            return;
        }

        std::vector<std::pair<BimDocument*, uint32_t>> allSubMeshes;
        for (auto& doc : documents) {
            for (uint32_t i=0; i<doc->GetGeometry().subMeshes.size(); ++i) {
                allSubMeshes.push_back({doc.get(), i});
            }
        }

        bool enterPressed = ImGui::InputTextWithHint("##globSearch", ICON_FA_SEARCH " Search All Models...", state.globalSearchBuf, sizeof(state.globalSearchBuf), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("Search") || enterPressed) {
            std::string query = state.globalSearchBuf;
            if (!query.empty() && !state.isSearching.load()) {
                state.isSearchActive = true;
                state.isSearching.store(true);

                std::thread([&state, query, allSubMeshes]() {
                    std::vector<SearchResult> results;
                    for (uint32_t i = 0; i < allSubMeshes.size(); ++i) {
                        auto doc = allSubMeshes[i].first;
                        const auto& sub = doc->GetGeometry().subMeshes[allSubMeshes[i].second];

                        if (!state.showOpeningsAndSpaces && (sub.type == "IfcOpeningElement" || sub.type == "IfcSpace")) continue;

                        std::string nameSearchTarget = sub.type;
                        if (state.cachedNames.count(sub.guid)) nameSearchTarget = state.cachedNames[sub.guid];

                        if (icontains(nameSearchTarget, query)) {
                            results.push_back({i, "Type/Name", "Name", nameSearchTarget});
                            continue;
                        }
                        if (icontains(sub.guid, query)) {
                            results.push_back({i, "GUID", "GUID", sub.guid});
                            continue;
                        }
                        auto props = doc->GetElementProperties(sub.guid);
                        for (const auto& [pk, pv] : props) {
                            if (icontains(pk, query) || icontains(pv.value, query)) {
                                results.push_back({i, "Property", pk, pv.value});
                                break;
                            }
                        }
                    }
                    std::lock_guard<std::mutex> lock(state.searchMutex);
                    state.searchResults = std::move(results);
                    state.isSearching.store(false);
                    state.lastClickedVisualIndex = -1;
                }).detach();
            } else if (query.empty()) {
                state.isSearchActive = false;
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            memset(state.globalSearchBuf, 0, sizeof(state.globalSearchBuf));
            state.isSearchActive = false;
            std::lock_guard<std::mutex> lock(state.searchMutex);
            state.searchResults.clear();
            state.lastClickedVisualIndex = -1;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("Model Tree View");
        ImGui::BeginChild("ModelTree", ImVec2(0, 0), true);

        ImVec2 sqBtn(ImGui::GetFrameHeight(), ImGui::GetFrameHeight());

        if (state.isSearchActive) {
            if (state.isSearching.load()) {
                ImGui::TextDisabled("Searching across %zu elements...", allSubMeshes.size());
            } else {
                std::lock_guard<std::mutex> lock(state.searchMutex);

                if (ImGui::Button(ICON_FA_DOWNLOAD " Export CSV")) {
                    auto fd = pfd::save_file("Export Results", "Search_Results.csv", { "CSV Files", "*.csv" });
                    std::string path = fd.result();
                    if (!path.empty()) {
                        std::ofstream out(path);
                        out << "GUID,Type,Match Type,Match Key,Match Value\n";
                        for (auto& res : state.searchResults) {
                            auto doc = allSubMeshes[res.subMeshIndex].first;
                            const auto& sub = doc->GetGeometry().subMeshes[allSubMeshes[res.subMeshIndex].second];
                            out << sub.guid << ","
                            << sub.type << ","
                            << res.matchType << ","
                            << "\"" << res.matchKey << "\",\"" << res.matchValue << "\"\n";
                        }
                    }
                }

                ImGui::Text("Found %zu matches:", state.searchResults.size());
                ImGui::Separator();

                std::vector<uint32_t> searchMeshIndices;
                searchMeshIndices.reserve(state.searchResults.size());
                for (auto& r : state.searchResults) searchMeshIndices.push_back(r.subMeshIndex);

                if (ImGui::BeginTable("##search_table", 1, ImGuiTableFlags_SizingFixedFit)) {
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);

                    ImGuiListClipper clipper;
                    clipper.Begin(static_cast<int>(state.searchResults.size()));
                    while (clipper.Step()) {
                        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);

                            const auto& res = state.searchResults[i];
                            auto doc = allSubMeshes[res.subMeshIndex].first;
                            const auto& sub = doc->GetGeometry().subMeshes[allSubMeshes[res.subMeshIndex].second];

                            if (state.cachedNames.find(sub.guid) == state.cachedNames.end()) {
                                auto props = doc->GetElementProperties(sub.guid);
                                state.cachedNames[sub.guid] = (props.count("Name") && !props["Name"].value.empty()) ? props["Name"].value : sub.type;
                            }

                            std::string shortGuid = sub.guid.length() >= 8 ? sub.guid.substr(sub.guid.length() - 8) : sub.guid;
                            std::string snippet = res.matchType == "Property" ? (res.matchKey + ": " + res.matchValue) : res.matchValue;
                            std::string label = state.cachedNames[sub.guid] + " [" + shortGuid + "] - " + snippet + "###" + sub.guid;

                            bool isSelected = std::any_of(state.objects.begin(), state.objects.end(), [&](const SelectedObject& o) { return o.guid == sub.guid; });

                            ImVec4 hoverColor = isSelected ? ImGui::GetStyleColorVec4(ImGuiCol_Header) : ImVec4(0,0,0,0);
                            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, hoverColor);
                            if (ImGui::Selectable(label.c_str(), isSelected)) {
                                std::shared_ptr<BimDocument> targetDocPtr;
                                for (auto d : documents) if (d.get() == doc) targetDocPtr = d;
                                HandleShiftSelection(state, i, allSubMeshes[res.subMeshIndex].second, "SEARCH_RES", searchMeshIndices, targetDocPtr);
                            }
                            ImGui::PopStyleColor();

                            if (isSelected && triggerFocus) ImGui::SetScrollHereY(0.5f);
                        }
                    }
                    ImGui::EndTable();
                }
            }
        } else {
            for (auto it = documents.begin(); it != documents.end(); ) {
                auto& doc = *it;
                std::string filename = std::filesystem::path(doc->GetFilePath()).filename().string();

                // --- FIXED: Display (Hidden) next to the filename ---
                if (doc->IsHidden()) filename += " (Hidden)";

                ImGui::PushID(doc.get());
                bool isFileNodeOpen = ImGui::TreeNodeEx("FileNode", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen, "%s", filename.c_str());

                if (ImGui::BeginPopupContextItem()) {
                    // --- FIXED: Context menu options to Hide/Show model ---
                    if (doc->IsHidden()) {
                        if (ImGui::MenuItem("Show model")) doc->SetHidden(false);
                    } else {
                        if (ImGui::MenuItem("Hide model")) {
                            doc->SetHidden(true);
                            // Clear selections for this hidden model
                            state.objects.erase(std::remove_if(state.objects.begin(), state.objects.end(),
                                                               [&](const SelectedObject& o) {
                                                                   for (const auto& sub : doc->GetGeometry().subMeshes) { if (o.guid == sub.guid) return true; } return false;
                                                               }), state.objects.end());
                        }
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Rotate +90° X (Flip Up)")) {
                        doc->ApplyTransform(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1, 0, 0)));
                        triggerRebuild = true;
                    }
                    if (ImGui::MenuItem("Rotate -90° X")) {
                        doc->ApplyTransform(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1, 0, 0)));
                        triggerRebuild = true;
                    }
                    if (ImGui::MenuItem("Rotate +90° Y")) {
                        doc->ApplyTransform(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0, 1, 0)));
                        triggerRebuild = true;
                    }
                    if (ImGui::MenuItem("Rotate +90° Z")) {
                        doc->ApplyTransform(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0, 0, 1)));
                        triggerRebuild = true;
                    }
                    ImGui::Separator();

                    // --- FIXED: Rename to "Close model" ---
                    if (ImGui::MenuItem("Close model")) {
                        it = documents.erase(it);
                        state.objects.clear();
                        triggerRebuild = true;
                        ImGui::EndPopup();
                        if (isFileNodeOpen) ImGui::TreePop();
                        ImGui::PopID();
                        continue;
                    }
                    ImGui::EndPopup();
                }

                if (isFileNodeOpen) {
                    if (doc->IsHidden()) {
                        ImGui::TextDisabled("Model is currently hidden.");
                    } else {
                        std::map<std::string, std::vector<uint32_t>> docGroups;
                        const auto& subMeshes = doc->GetGeometry().subMeshes;
                        for (uint32_t i = 0; i < subMeshes.size(); ++i) {
                            if (!state.showOpeningsAndSpaces && (subMeshes[i].type == "IfcOpeningElement" || subMeshes[i].type == "IfcSpace")) continue;
                            docGroups[subMeshes[i].type].push_back(i);
                        }

                        for (const auto& [type, indices] : docGroups) {
                            bool groupHasHidden = false, groupHasDeleted = false, groupHasEdited = false, groupHasSelected = false;
                            for (uint32_t idx : indices) {
                                const auto& sub = subMeshes[idx];
                                if (state.hiddenObjects.count(sub.guid)) groupHasHidden = true;
                                if (state.deletedObjects.count(sub.guid)) groupHasDeleted = true;
                                if (doc->HasModifiedProperties(sub.guid)) groupHasEdited = true;
                                if (!groupHasSelected && std::any_of(state.objects.begin(), state.objects.end(), [&](const SelectedObject& o) { return o.guid == sub.guid; })) {
                                    groupHasSelected = true;
                                }
                            }

                            std::string extraTags = "";
                            if (groupHasHidden) extraTags += " (hidden elements)";
                            if (groupHasDeleted) extraTags += " (deleted elements)";
                            if (groupHasEdited) extraTags += " (edited elements)";

                            if (groupHasSelected) ImGui::SetNextItemOpen(true, ImGuiCond_Always);

                            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.3f, 0.45f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.35f, 0.5f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.25f, 0.4f, 0.55f, 1.0f));
                            bool isNodeOpen = ImGui::TreeNodeEx(type.c_str(), ImGuiTreeNodeFlags_Framed, "%s (%zu)%s", type.c_str(), indices.size(), extraTags.c_str());
                            ImGui::PopStyleColor(3);

                            if (isNodeOpen) {
                                if (ImGui::BeginTable(("##table_" + type).c_str(), 2, ImGuiTableFlags_SizingFixedFit)) {
                                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                                    ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 25.0f);

                                    ImGuiListClipper clipper;
                                    clipper.Begin(static_cast<int>(indices.size()));
                                    while (clipper.Step()) {
                                        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                                            const auto& sub = subMeshes[indices[i]];

                                            ImGui::TableNextRow();
                                            ImGui::TableSetColumnIndex(0);

                                            if (state.cachedNames.find(sub.guid) == state.cachedNames.end()) {
                                                auto props = doc->GetElementProperties(sub.guid);
                                                state.cachedNames[sub.guid] = (props.count("Name") && !props["Name"].value.empty()) ? props["Name"].value : sub.type;
                                            }

                                            bool isHidden = state.hiddenObjects.count(sub.guid) > 0;
                                            bool isDeleted = state.deletedObjects.count(sub.guid) > 0;
                                            bool isEdited = doc->HasModifiedProperties(sub.guid);

                                            std::string status = "";
                                            if (isEdited && !isDeleted) status = " (Edited)";

                                            std::string shortGuid = sub.guid.length() >= 8 ? sub.guid.substr(sub.guid.length() - 8) : sub.guid;
                                            std::string label = state.cachedNames[sub.guid] + " [" + shortGuid + "]" + status + "###" + sub.guid;

                                            bool isSelected = std::any_of(state.objects.begin(), state.objects.end(), [&](const SelectedObject& o) { return o.guid == sub.guid; });

                                            if (isDeleted) {
                                                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                                            } else if (isHidden) {
                                                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                                            }

                                            ImVec4 hoverColor = isSelected ? ImGui::GetStyleColorVec4(ImGuiCol_Header) : ImVec4(0,0,0,0);
                                            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, hoverColor);
                                            if (ImGui::Selectable(label.c_str(), isSelected) && !isDeleted) {
                                                HandleShiftSelection(state, i, indices[i], type + "_" + filename, indices, doc);
                                            }
                                            ImGui::PopStyleColor();

                                            if (isSelected && triggerFocus) ImGui::SetScrollHereY(0.5f);
                                            if (isDeleted || isHidden) ImGui::PopStyleColor();

                                            if (isDeleted) {
                                                ImVec2 min = ImGui::GetItemRectMin();
                                                ImVec2 max = ImGui::GetItemRectMax();
                                                float midY = (min.y + max.y) * 0.5f;
                                                ImGui::GetWindowDrawList()->AddLine(ImVec2(min.x, midY), ImVec2(max.x, midY), IM_COL32(200, 50, 50, 255), 1.5f);
                                            }

                                            ImGui::TableSetColumnIndex(1);
                                            if (isDeleted) {
                                                ImGui::PushID((sub.guid + "_undo").c_str());
                                                if (ImGui::Button(ICON_FA_UNDO, sqBtn)) {
                                                    state.deletedObjects.erase(sub.guid);
                                                    state.hiddenObjects.erase(sub.guid);
                                                    state.hiddenStateChanged = true;
                                                }
                                                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Undo delete");
                                                ImGui::PopID();
                                            } else if (isHidden) {
                                                ImGui::PushID((sub.guid + "_show").c_str());
                                                if (ImGui::Button(ICON_FA_EYE, sqBtn)) {
                                                    state.hiddenObjects.erase(sub.guid);
                                                    state.hiddenStateChanged = true;
                                                }
                                                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show element");
                                                ImGui::PopID();
                                            }
                                        }
                                    }
                                    ImGui::EndTable();
                                }
                                ImGui::TreePop();
                            }
                        }
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
                ++it;
            }
        }
        ImGui::EndChild();
        ImGui::End();
    }

    void UIMainPanel::HandleShiftSelection(SelectionState& state, int visualIdx, uint32_t meshIdx, const std::string& groupName, const std::vector<uint32_t>& currentArray, std::shared_ptr<BimDocument> document) {
        ImGuiIO& io = ImGui::GetIO();
        const auto& subMeshes = document->GetGeometry().subMeshes;

        if (io.KeyShift && state.lastClickedVisualIndex != -1 && state.lastClickedGroup == groupName) {
            if (!io.KeyCtrl) state.objects.clear();
            int start = std::min(state.lastClickedVisualIndex, visualIdx);
            int end = std::max(state.lastClickedVisualIndex, visualIdx);
            for (int j = start; j <= end; ++j) {
                uint32_t targetMeshIdx = currentArray[j];
                const auto& targetSub = subMeshes[targetMeshIdx];
                bool isSel = std::any_of(state.objects.begin(), state.objects.end(), [&](const SelectedObject& o) { return o.guid == targetSub.guid; });
                if (!isSel) {
                    SelectedObject so; so.guid = targetSub.guid; so.type = targetSub.type;
                    so.startIndex = targetSub.globalStartIndex; so.indexCount = targetSub.indexCount;
                    so.properties = document->GetElementProperties(targetSub.guid);
                    state.objects.push_back(so);
                }
            }
        } else {
            if (!io.KeyCtrl) state.objects.clear();
            bool isSelected = std::any_of(state.objects.begin(), state.objects.end(), [&](const SelectedObject& o) { return o.guid == subMeshes[meshIdx].guid; });
            if (!isSelected) {
                const auto& targetSub = subMeshes[meshIdx];
                SelectedObject so; so.guid = targetSub.guid; so.type = targetSub.type;
                so.startIndex = targetSub.globalStartIndex; so.indexCount = targetSub.indexCount;
                so.properties = document->GetElementProperties(targetSub.guid);
                state.objects.push_back(so);
            } else if (io.KeyCtrl) {
                state.objects.erase(std::remove_if(state.objects.begin(), state.objects.end(), [&](const SelectedObject& o) { return o.guid == subMeshes[meshIdx].guid; }), state.objects.end());
            }
        }
        state.lastClickedVisualIndex = visualIdx;
        state.lastClickedGroup = groupName;
        state.selectionChanged = true;
    }

    void UIMainPanel::DrawResetModal(SelectionState& state, std::vector<std::shared_ptr<BimDocument>>& documents, bool& triggerRebuild) {
        if (ImGui::BeginPopupModal("Reset Model", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("This will reset all items back to original.\nAre you sure?");
            ImGui::Separator();

            if (ImGui::Button("OK", ImVec2(120, 0))) {
                for (auto& doc : documents) {
                    doc->SetHidden(false); // --- FIXED: Show all parent models on reset ---
                    for (auto& [guid, props] : state.originalProperties) {
                        for (auto& [k, v] : props) doc->UpdateElementProperty(guid, k, v);
                    }
                }

                triggerRebuild = true;

                state.explodeFactor = 0.0f;
                state.updateGeometry = true;
                memset(state.globalSearchBuf, 0, sizeof(state.globalSearchBuf));
                memset(state.localSearchBuf, 0, sizeof(state.localSearchBuf));

                state.originalProperties.clear();
                state.deletedProperties.clear();
                state.deletedObjects.clear();
                state.hiddenObjects.clear();
                state.objects.clear();
                state.hiddenStateChanged = true;
                state.triggerResetCamera = true;
                state.selectionChanged = true;

                state.measureToolActive = false;
                state.completedMeasurements.clear();
                state.isMeasuringActive = false;

                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
    }
}
