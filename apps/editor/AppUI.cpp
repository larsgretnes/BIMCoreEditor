// =============================================================================
// BimCore/apps/editor/AppUI.cpp
// =============================================================================
#include "AppUI.h"
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_wgpu.h>
#include <algorithm>
#include <thread>
#include <cctype>
#include <fstream>
#include "platform/portable-file-dialogs.h"

// FontAwesome Icons
#define ICON_FA_FOLDER_OPEN   "\xef\x81\xbc"
#define ICON_FA_SAVE          "\xef\x83\x87"
#define ICON_FA_MOUSE_POINTER "\xef\x89\x85"
#define ICON_FA_ARROWS_ALT    "\xef\x82\xb2"
#define ICON_FA_SYNC          "\xef\x80\xa1"
#define ICON_FA_EDIT          "\xef\x8c\x83"
#define ICON_FA_TRASH         "\xef\x80\x8d"
#define ICON_FA_CHECK         "\xef\x80\x8c"
#define ICON_FA_BAN           "\xef\x81\x9e"
#define ICON_FA_UNDO          "\xef\x80\x9e"
#define ICON_FA_SEARCH        "\xef\x80\x82"
#define ICON_FA_DOWNLOAD      "\xef\x80\x99"
#define ICON_FA_TIMES_CIRCLE  "\xef\x81\x97"
#define ICON_FA_EYE           "\xef\x81\xae"
#define ICON_FA_HISTORY       "\xef\x87\xba"

namespace BimCore {

    static bool icontains(const std::string& str, const std::string& query) {
        if (query.empty()) return true;
        auto it = std::search(str.begin(), str.end(), query.begin(), query.end(),
                              [](unsigned char ch1, unsigned char ch2) { return std::tolower(ch1) == std::tolower(ch2); });
        return it != str.end();
    }

    void AppUI::NewFrame() {
        ImGui_ImplWGPU_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void AppUI::Render(SelectionState& selection, GraphicsContext& graphics, std::shared_ptr<BimDocument> document, Camera& camera, float configMaxExplode, bool& triggerFocus, bool isFlightMode) {
        ImGuiIO& io = ImGui::GetIO();
        const ImGuiViewport* viewport = ImGui::GetMainViewport();

        if (isFlightMode) {
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

        if (!selection.showUI) {
            ImGui::Render();
            return;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));

        bool editingActiveAtStartOfFrame = !selection.activeEditGuid.empty();
        bool isActivelyLoading = selection.loadState && !selection.loadState->isLoaded.load() && selection.loadState->progress.load() > 0.0f;

        const float leftPanelWidth = 400.0f;
        const float rightPanelWidth = 450.0f;
        const float statsPanelHeight = 150.0f;
        const float mainPanelHeight = viewport->WorkSize.y - statsPanelHeight;

        // Base square dimension for standard grid icons (1.0x scale)
        ImVec2 sqBtn(ImGui::GetFrameHeight(), ImGui::GetFrameHeight());

        auto handleShiftSelection = [&](int visualIdx, uint32_t meshIdx, const std::string& groupName, const std::vector<uint32_t>& currentArray, const std::vector<RenderSubMesh>& subMeshes) {
            if (io.KeyShift && selection.lastClickedVisualIndex != -1 && selection.lastClickedGroup == groupName) {
                if (!io.KeyCtrl) selection.objects.clear();
                int start = std::min(selection.lastClickedVisualIndex, visualIdx);
                int end = std::max(selection.lastClickedVisualIndex, visualIdx);
                for (int j = start; j <= end; ++j) {
                    uint32_t targetMeshIdx = currentArray[j];
                    const auto& targetSub = subMeshes[targetMeshIdx];
                    bool isSel = std::any_of(selection.objects.begin(), selection.objects.end(), [&](const SelectedObject& o) { return o.guid == targetSub.guid; });
                    if (!isSel) {
                        SelectedObject so; so.guid = targetSub.guid; so.type = targetSub.type; so.startIndex = targetSub.startIndex; so.indexCount = targetSub.indexCount; so.properties = document->GetElementProperties(targetSub.guid);
                        selection.objects.push_back(so);
                    }
                }
            } else {
                if (!io.KeyCtrl) selection.objects.clear();
                bool isSelected = std::any_of(selection.objects.begin(), selection.objects.end(), [&](const SelectedObject& o) { return o.guid == subMeshes[meshIdx].guid; });
                if (!isSelected) {
                    const auto& targetSub = subMeshes[meshIdx];
                    SelectedObject so; so.guid = targetSub.guid; so.type = targetSub.type; so.startIndex = targetSub.startIndex; so.indexCount = targetSub.indexCount; so.properties = document->GetElementProperties(targetSub.guid);
                    selection.objects.push_back(so);
                } else if (io.KeyCtrl) {
                    selection.objects.erase(std::remove_if(selection.objects.begin(), selection.objects.end(), [&](const SelectedObject& o) { return o.guid == subMeshes[meshIdx].guid; }), selection.objects.end());
                }
            }
            selection.lastClickedVisualIndex = visualIdx;
            selection.lastClickedGroup = groupName;
        };

        // =====================================================================
        // 1. MAIN PANEL (Top Left)
        // =====================================================================
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y), ImGuiCond_Always);
        ImGui::SetNextWindowSizeConstraints(ImVec2(300, mainPanelHeight), ImVec2(viewport->WorkSize.x / 2.0f, mainPanelHeight));
        ImGui::SetNextWindowSize(ImVec2(leftPanelWidth, mainPanelHeight), ImGuiCond_FirstUseEver);

        ImGui::Begin("Main Menu", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

        ImGui::SetWindowFontScale(1.5f);

        float bigBtnDim = ImGui::GetFrameHeight();
        ImVec2 bigBtnSize(bigBtnDim, bigBtnDim);

        if (ImGui::Button(ICON_FA_FOLDER_OPEN, bigBtnSize)) selection.triggerLoad = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Open");
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_SAVE, bigBtnSize)) selection.triggerSave = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save As");

        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float rightButtonGroupWidth = (bigBtnDim * 3.0f) + (spacing * 2.0f);
        float cursorX = ImGui::GetWindowContentRegionMax().x - rightButtonGroupWidth;

        if (cursorX > ImGui::GetCursorPosX()) ImGui::SameLine(cursorX);
        else ImGui::SameLine();

        auto drawToolBtn = [&](InteractionTool tool, const char* icon, const char* id) {
            if (selection.activeTool == tool) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button(icon, bigBtnSize)) selection.activeTool = tool;
            if (selection.activeTool == tool) ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", id);
        };

            drawToolBtn(InteractionTool::Select, ICON_FA_MOUSE_POINTER, "Select (1)"); ImGui::SameLine();
            drawToolBtn(InteractionTool::Pan, ICON_FA_ARROWS_ALT, "Pan (2)"); ImGui::SameLine();
            drawToolBtn(InteractionTool::Orbit, ICON_FA_SYNC, "Orbit (3)");

            ImGui::Spacing();
            if (ImGui::Button(ICON_FA_TIMES_CIRCLE, bigBtnSize)) { selection.objects.clear(); }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear selected");
            ImGui::SameLine();
        if (ImGui::Button(ICON_FA_EYE, bigBtnSize)) { selection.hiddenObjects.clear(); selection.hiddenStateChanged = true; }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show all");
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_HISTORY, bigBtnSize)) { ImGui::OpenPopup("Reset Model"); }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset");

        ImGui::SetWindowFontScale(1.0f);

        if (ImGui::BeginPopupModal("Reset Model", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("This will reset all items back to original.\nAre you sure?");
            ImGui::Separator();

            if (ImGui::Button("OK", ImVec2(120, 0))) {
                if (document) {
                    for (auto& [guid, props] : selection.originalProperties) {
                        for (auto& [k, v] : props) document->UpdateElementProperty(guid, k, v);
                    }

                    selection.clipX = document->GetGeometry().maxBounds[0] + 0.1f;
                    selection.clipY = document->GetGeometry().maxBounds[1] + 0.1f;
                    selection.clipZ = document->GetGeometry().maxBounds[2] + 0.1f;
                }

                selection.explodeFactor = 0.0f;
                selection.updateGeometry = true;
                memset(selection.globalSearchBuf, 0, sizeof(selection.globalSearchBuf));
                memset(selection.localSearchBuf, 0, sizeof(selection.localSearchBuf));

                selection.originalProperties.clear();
                selection.deletedProperties.clear();
                selection.deletedObjects.clear();
                selection.hiddenObjects.clear();
                selection.objects.clear();
                selection.hiddenStateChanged = true;
                selection.triggerResetCamera = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }

        ImGui::Separator();

        // --- EXPLODE SLIDER ---
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.30f, 0.35f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.30f, 0.35f, 0.40f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.35f, 0.40f, 0.45f, 1.0f));
        bool isExplodeOpen = ImGui::CollapsingHeader("Explode");
        ImGui::PopStyleColor(3);

        if (isExplodeOpen) {
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SliderFloat("##explode", &selection.explodeFactor, 0.0f, configMaxExplode, "%.2fx");
            if (ImGui::IsItemActive()) selection.updateGeometry = true;
        }

        // --- CLIPPING PLANES ---
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.30f, 0.35f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.30f, 0.35f, 0.40f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.35f, 0.40f, 0.45f, 1.0f));
        bool isClippingOpen = ImGui::CollapsingHeader("Clipping Planes");
        ImGui::PopStyleColor(3);

        if (isClippingOpen) {
            float bMinX = -100.0f, bMaxX = 100.0f;
            float bMinY = -100.0f, bMaxY = 100.0f;
            float bMinZ = -100.0f, bMaxZ = 100.0f;

            if (document) {
                bMinX = document->GetGeometry().minBounds[0] - 0.1f; bMaxX = document->GetGeometry().maxBounds[0] + 0.1f;
                bMinY = document->GetGeometry().minBounds[1] - 0.1f; bMaxY = document->GetGeometry().maxBounds[1] + 0.1f;
                bMinZ = document->GetGeometry().minBounds[2] - 0.1f; bMaxZ = document->GetGeometry().maxBounds[2] + 0.1f;
            }

            auto drawClipRow = [](const char* axis, bool& show, float& val, float minB, float maxB, float* col) {
                ImGui::PushID(axis);

                ImGui::Checkbox("##show", &show);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle Glass Plane Visibility");
                ImGui::SameLine();

                float colorBtnW = ImGui::GetFrameHeight();
                float spacing = ImGui::GetStyle().ItemSpacing.x;
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - colorBtnW - spacing);

                std::string format = std::string(axis) + ": %.2f";
                ImGui::SliderFloat("##val", &val, minB, maxB, format.c_str());

                ImGui::SameLine();
                ImGui::ColorEdit3("##col", col, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoAlpha);

                ImGui::PopID();
            };

            drawClipRow("X", selection.showPlaneX, selection.clipX, bMinX, bMaxX, selection.planeColorX);
            drawClipRow("Y", selection.showPlaneY, selection.clipY, bMinY, bMaxY, selection.planeColorY);
            drawClipRow("Z", selection.showPlaneZ, selection.clipZ, bMinZ, bMaxZ, selection.planeColorZ);
        }
        ImGui::Separator();

        // --- GLOBAL SEARCH & TREE VIEW ---
        if (document) {
            const auto& subMeshes = document->GetGeometry().subMeshes;

            if (!selection.groupsBuilt) {
                selection.cachedGroups.clear();
                selection.cachedNames.clear();
                for (uint32_t i = 0; i < subMeshes.size(); ++i) {
                    selection.cachedGroups[subMeshes[i].type].push_back(i);
                }
                selection.groupsBuilt = true;
            }

            bool enterPressed = ImGui::InputTextWithHint("##globSearch", ICON_FA_SEARCH " Search Model...", selection.globalSearchBuf, sizeof(selection.globalSearchBuf), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            if (ImGui::Button("Search") || enterPressed) {
                std::string query = selection.globalSearchBuf;
                if (!query.empty() && !selection.isSearching.load()) {
                    selection.isSearchActive = true;
                    selection.isSearching.store(true);

                    std::thread([&selection, document, query, subMeshes]() {
                        std::vector<SearchResult> results;
                        for (uint32_t i = 0; i < subMeshes.size(); ++i) {
                            const auto& sub = subMeshes[i];

                            std::string nameSearchTarget = sub.type;
                            if (selection.cachedNames.count(sub.guid)) nameSearchTarget = selection.cachedNames[sub.guid];

                            if (icontains(nameSearchTarget, query)) {
                                results.push_back({i, "Type/Name", "Name", nameSearchTarget});
                                continue;
                            }
                            if (icontains(sub.guid, query)) {
                                results.push_back({i, "GUID", "GUID", sub.guid});
                                continue;
                            }
                            auto props = document->GetElementProperties(sub.guid);
                            for (const auto& [pk, pv] : props) {
                                if (icontains(pk, query) || icontains(pv.value, query)) {
                                    results.push_back({i, "Property", pk, pv.value});
                                    break;
                                }
                            }
                        }
                        std::lock_guard<std::mutex> lock(selection.searchMutex);
                        selection.searchResults = std::move(results);
                        selection.isSearching.store(false);
                        selection.lastClickedVisualIndex = -1;
                    }).detach();
                } else if (query.empty()) {
                    selection.isSearchActive = false;
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                memset(selection.globalSearchBuf, 0, sizeof(selection.globalSearchBuf));
                selection.isSearchActive = false;
                std::lock_guard<std::mutex> lock(selection.searchMutex);
                selection.searchResults.clear();
                selection.lastClickedVisualIndex = -1;
            }

            ImGui::BeginChild("ModelTree", ImVec2(0, 0), true);

            if (selection.isSearchActive) {
                if (selection.isSearching.load()) {
                    ImGui::TextDisabled("Searching across %zu elements...", subMeshes.size());
                } else {
                    std::lock_guard<std::mutex> lock(selection.searchMutex);

                    if (ImGui::Button(ICON_FA_DOWNLOAD " Export CSV")) {
                        auto fd = pfd::save_file("Export Results", "Search_Results.csv", { "CSV Files", "*.csv" });
                        std::string path = fd.result();
                        if (!path.empty()) {
                            std::ofstream out(path);
                            out << "GUID,Type,Match Type,Match Key,Match Value\n";
                            for (auto& res : selection.searchResults) {
                                out << subMeshes[res.subMeshIndex].guid << ","
                                << subMeshes[res.subMeshIndex].type << ","
                                << res.matchType << ","
                                << "\"" << res.matchKey << "\",\"" << res.matchValue << "\"\n";
                            }
                        }
                    }

                    ImGui::Text("Found %zu matches:", selection.searchResults.size());
                    ImGui::Separator();

                    std::vector<uint32_t> searchMeshIndices;
                    searchMeshIndices.reserve(selection.searchResults.size());
                    for (auto& r : selection.searchResults) searchMeshIndices.push_back(r.subMeshIndex);

                    if (ImGui::BeginTable("##search_table", 1, ImGuiTableFlags_SizingFixedFit)) {
                        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);

                        ImGuiListClipper clipper;
                        clipper.Begin(static_cast<int>(selection.searchResults.size()));
                        while (clipper.Step()) {
                            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);

                                const auto& res = selection.searchResults[i];
                                const auto& sub = subMeshes[res.subMeshIndex];

                                if (selection.cachedNames.find(sub.guid) == selection.cachedNames.end()) {
                                    auto props = document->GetElementProperties(sub.guid);
                                    selection.cachedNames[sub.guid] = (props.count("Name") && !props["Name"].value.empty()) ? props["Name"].value : sub.type;
                                }

                                std::string shortGuid = sub.guid.length() >= 8 ? sub.guid.substr(sub.guid.length() - 8) : sub.guid;
                                std::string snippet = res.matchType == "Property" ? (res.matchKey + ": " + res.matchValue) : res.matchValue;
                                std::string label = selection.cachedNames[sub.guid] + " [" + shortGuid + "] - " + snippet + "###" + sub.guid;

                                bool isSelected = std::any_of(selection.objects.begin(), selection.objects.end(), [&](const SelectedObject& o) { return o.guid == sub.guid; });

                                ImVec4 hoverColor = isSelected ? ImGui::GetStyleColorVec4(ImGuiCol_Header) : ImVec4(0,0,0,0);
                                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, hoverColor);
                                if (ImGui::Selectable(label.c_str(), isSelected)) {
                                    handleShiftSelection(i, res.subMeshIndex, "SEARCH_RES", searchMeshIndices, subMeshes);
                                }
                                ImGui::PopStyleColor();

                                if (isSelected && triggerFocus) ImGui::SetScrollHereY(0.5f);
                            }
                        }
                        ImGui::EndTable();
                    }
                }
            } else {
                for (const auto& [type, indices] : selection.cachedGroups) {

                    bool groupHasHidden = false, groupHasDeleted = false, groupHasEdited = false, groupHasSelected = false;
                    for (uint32_t idx : indices) {
                        const auto& sub = subMeshes[idx];
                        if (selection.hiddenObjects.count(sub.guid)) groupHasHidden = true;
                        if (selection.deletedObjects.count(sub.guid)) groupHasDeleted = true;
                        if (document->HasModifiedProperties(sub.guid)) groupHasEdited = true;
                        if (!groupHasSelected && std::any_of(selection.objects.begin(), selection.objects.end(), [&](const SelectedObject& o) { return o.guid == sub.guid; })) {
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

                                    if (selection.cachedNames.find(sub.guid) == selection.cachedNames.end()) {
                                        auto props = document->GetElementProperties(sub.guid);
                                        selection.cachedNames[sub.guid] = (props.count("Name") && !props["Name"].value.empty()) ? props["Name"].value : sub.type;
                                    }

                                    bool isHidden = selection.hiddenObjects.count(sub.guid) > 0;
                                    bool isDeleted = selection.deletedObjects.count(sub.guid) > 0;
                                    bool isEdited = document->HasModifiedProperties(sub.guid);

                                    std::string status = "";
                                    if (isEdited && !isDeleted) status = " (Edited)";

                                    std::string shortGuid = sub.guid.length() >= 8 ? sub.guid.substr(sub.guid.length() - 8) : sub.guid;
                                    std::string label = selection.cachedNames[sub.guid] + " [" + shortGuid + "]" + status + "###" + sub.guid;

                                    bool isSelected = std::any_of(selection.objects.begin(), selection.objects.end(), [&](const SelectedObject& o) { return o.guid == sub.guid; });

                                    if (isDeleted) {
                                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                                    } else if (isHidden) {
                                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                                    }

                                    ImVec4 hoverColor = isSelected ? ImGui::GetStyleColorVec4(ImGuiCol_Header) : ImVec4(0,0,0,0);
                                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, hoverColor);
                                    if (ImGui::Selectable(label.c_str(), isSelected) && !isDeleted) {
                                        handleShiftSelection(i, indices[i], type, indices, subMeshes);
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
                                            selection.deletedObjects.erase(sub.guid);
                                            selection.hiddenObjects.erase(sub.guid);
                                            selection.hiddenStateChanged = true;
                                        }
                                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Undo delete");
                                        ImGui::PopID();
                                    } else if (isHidden) {
                                        ImGui::PushID((sub.guid + "_show").c_str());
                                        if (ImGui::Button(ICON_FA_EYE, sqBtn)) {
                                            selection.hiddenObjects.erase(sub.guid);
                                            selection.hiddenStateChanged = true;
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
            ImGui::EndChild();
        } else {
            ImGui::TextDisabled("No model loaded.");
        }

        float currentLeftWidth = ImGui::GetWindowWidth();
        ImGui::End();

        // =====================================================================
        // 2. STATUS PANEL (Bottom Left)
        // =====================================================================
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + mainPanelHeight), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(currentLeftWidth, statsPanelHeight), ImGuiCond_Always);
        ImGui::Begin("Status", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

        if (isActivelyLoading) {
            ImGui::Text("%s", selection.loadState->GetStatus().c_str());
            ImGui::ProgressBar(selection.loadState->progress.load());
            ImGui::Separator();
        }

        ImGui::Text("FPS: %.1f", io.Framerate);
        if (document) {
            ImGui::Text("Elements: %zu", document->GetGeometry().subMeshes.size());
            ImGui::Text("Vertices: %zu", document->GetGeometry().vertices.size());
            ImGui::Text("Selected: %zu", selection.objects.size());
        }
        ImGui::End();

        // =====================================================================
        // 3. PROPERTIES PANEL (Right)
        // =====================================================================
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x, viewport->WorkPos.y), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        ImGui::SetNextWindowSizeConstraints(ImVec2(300, viewport->WorkSize.y), ImVec2(viewport->WorkSize.x / 2.0f, viewport->WorkSize.y));
        ImGui::SetNextWindowSize(ImVec2(rightPanelWidth, viewport->WorkSize.y), ImGuiCond_FirstUseEver);

        ImGui::Begin("Properties", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

        if (selection.objects.empty()) {
            ImGui::TextDisabled("Select an element to view properties.");
        } else {

            // --- TOP ROW: Search & Export ---
            float exportBtnWidth = 110.0f;
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - exportBtnWidth - ImGui::GetStyle().ItemSpacing.x);
            ImGui::InputTextWithHint("##locSearch", ICON_FA_SEARCH " Filter Properties...", selection.localSearchBuf, sizeof(selection.localSearchBuf));

            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_DOWNLOAD " Export CSV", ImVec2(exportBtnWidth, 0))) {
                auto fd = pfd::save_file("Export Selection", "Selection_Properties.csv", { "CSV Files", "*.csv" });
                std::string path = fd.result();
                if (!path.empty()) {
                    std::ofstream out(path);
                    out << "GUID,Type,Property,Value\n";
                    for (const auto& obj : selection.objects) {
                        for (const auto& [k, v] : obj.properties) {
                            out << obj.guid << "," << obj.type << ",\"" << k << "\",\"" << v.value << "\"\n";
                        }
                    }
                }
            }
            std::string locFilter = selection.localSearchBuf;
            ImGui::Separator();

            bool deleteAll = false;

            // --- GLOBAL ACTIONS ROW ---
            if (selection.objects.size() > 1) {
                if (ImGui::Button("Focus All")) { triggerFocus = true; }
                ImGui::SameLine();

                bool anyVisible = false;
                for (const auto& obj : selection.objects) {
                    if (selection.hiddenObjects.count(obj.guid) == 0) { anyVisible = true; break; }
                }

                if (ImGui::Button(anyVisible ? "Hide All" : "Show All")) {
                    for (const auto& obj : selection.objects) {
                        if (anyVisible) selection.hiddenObjects.insert(obj.guid);
                        else selection.hiddenObjects.erase(obj.guid);
                    }
                    selection.hiddenStateChanged = true;
                }
                ImGui::SameLine();

                if (ImGui::Button("Delete All")) { deleteAll = true; }

                ImGui::Separator();
            }

            if (deleteAll) {
                for (const auto& obj : selection.objects) {
                    selection.deletedObjects.insert(obj.guid);
                    selection.hiddenObjects.insert(obj.guid);
                }
                selection.objects.clear();
                selection.hiddenStateChanged = true;
            } else {
                std::string objToDeleteEntirely = "";
                std::string objToDeselect = "";
                bool globalRefreshNeeded = false;

                // --- SHARED PROPERTIES TREE ---
                if (selection.objects.size() > 1) {
                    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.4f, 0.3f, 0.1f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.45f, 0.35f, 0.15f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.5f, 0.4f, 0.2f, 1.0f));
                    bool isSharedOpen = ImGui::TreeNodeEx("Shared Properties", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed);
                    ImGui::PopStyleColor(3);

                    if (isSharedOpen) {
                        std::vector<std::string> sharedKeys;
                        std::map<std::string, std::string> sharedVals;
                        std::map<std::string, bool> isMulti;

                        bool first = true;
                        for (const auto& obj : selection.objects) {
                            if (first) {
                                for (const auto& [k, v] : obj.properties) {
                                    sharedKeys.push_back(k);
                                    sharedVals[k] = v.value;
                                    isMulti[k] = false;
                                }
                                first = false;
                            } else {
                                for (auto it = sharedKeys.begin(); it != sharedKeys.end(); ) {
                                    if (obj.properties.find(*it) == obj.properties.end()) {
                                        it = sharedKeys.erase(it);
                                    } else {
                                        if (sharedVals[*it] != obj.properties.at(*it).value) {
                                            isMulti[*it] = true;
                                        }
                                        ++it;
                                    }
                                }
                            }
                        }

                        if (sharedKeys.empty()) {
                            ImGui::TextDisabled("No shared properties found.");
                        } else {
                            if (ImGui::BeginTable("SharedPropTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                                ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch, 0.4f);
                                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.4f);
                                ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 65.0f);

                                for (const auto& key : sharedKeys) {
                                    if (!locFilter.empty() && !icontains(key, locFilter) && !icontains(sharedVals[key], locFilter)) continue;

                                    ImGui::TableNextRow();
                                    ImGui::TableSetColumnIndex(0); ImGui::TextWrapped("%s", key.c_str());
                                    ImGui::TableSetColumnIndex(1);

                                    ImGui::PushID(("Shared_" + key).c_str());
                                    if (selection.activeEditGuid == "SHARED" && selection.activeEditKey == key) {
                                        if (selection.focusEditField) { ImGui::SetKeyboardFocusHere(); selection.focusEditField = false; }

                                        bool enterPressed = ImGui::InputText("##edit", selection.editBuffer, sizeof(selection.editBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

                                        ImGui::TableSetColumnIndex(2);
                                        bool confirmPressed = ImGui::Button(ICON_FA_CHECK, sqBtn);
                                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Apply to all selected");

                                        ImGui::SameLine();
                                        if (ImGui::Button(ICON_FA_BAN, sqBtn)) { selection.activeEditGuid = ""; }
                                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cancel");

                                        if (enterPressed || confirmPressed) {
                                            for (auto& obj : selection.objects) {
                                                if (selection.originalProperties[obj.guid].find(key) == selection.originalProperties[obj.guid].end()) {
                                                    selection.originalProperties[obj.guid][key] = obj.properties[key].value;
                                                }
                                                document->UpdateElementProperty(obj.guid, key, selection.editBuffer);
                                            }
                                            selection.activeEditGuid = "";
                                            globalRefreshNeeded = true;
                                        }
                                    } else {
                                        if (isMulti[key]) ImGui::TextDisabled("<Multiple Values>");
                                        else ImGui::TextWrapped("%s", sharedVals[key].c_str());

                                        ImGui::TableSetColumnIndex(2);
                                        if (ImGui::Button(ICON_FA_EDIT, sqBtn)) {
                                            selection.activeEditGuid = "SHARED";
                                            selection.activeEditKey = key;
                                            strncpy(selection.editBuffer, isMulti[key] ? "" : sharedVals[key].c_str(), sizeof(selection.editBuffer));
                                            selection.focusEditField = true;
                                        }
                                        ImGui::SameLine();
                                        if (ImGui::Button(ICON_FA_TRASH, sqBtn)) {
                                            for (auto& obj : selection.objects) {
                                                if (selection.originalProperties[obj.guid].find(key) == selection.originalProperties[obj.guid].end()) {
                                                    selection.originalProperties[obj.guid][key] = obj.properties[key].value;
                                                }
                                                selection.deletedProperties[obj.guid].insert(key);
                                                document->UpdateElementProperty(obj.guid, key, "");
                                            }
                                            globalRefreshNeeded = true;
                                        }
                                    }
                                    ImGui::PopID();
                                }
                                ImGui::EndTable();
                            }
                        }
                        ImGui::TreePop();
                    }
                    ImGui::Separator();
                }

                if (globalRefreshNeeded) {
                    for (auto& obj : selection.objects) {
                        obj.properties = document->GetElementProperties(obj.guid);
                        if (obj.properties.count("Name") && !obj.properties["Name"].value.empty()) {
                            selection.cachedNames[obj.guid] = obj.properties["Name"].value;
                        }
                    }
                }

                // --- INDIVIDUAL OBJECT TREES ---
                ImGuiTreeNodeFlags nodeFlags = (selection.objects.size() == 1 || !locFilter.empty()) ? ImGuiTreeNodeFlags_DefaultOpen : 0;

                for (auto& obj : selection.objects) {
                    std::string shortGuid = obj.guid.length() >= 8 ? obj.guid.substr(obj.guid.length() - 8) : obj.guid;

                    std::string headerName = obj.type;
                    if (selection.cachedNames.count(obj.guid)) headerName = selection.cachedNames[obj.guid];

                    std::string treeId = obj.guid + "_tree";

                    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.3f, 0.45f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.35f, 0.5f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.25f, 0.4f, 0.55f, 1.0f));
                    bool isObjOpen = ImGui::TreeNodeEx(treeId.c_str(), nodeFlags | ImGuiTreeNodeFlags_Framed, "%s [%s]", headerName.c_str(), shortGuid.c_str());
                    ImGui::PopStyleColor(3);

                    if (isObjOpen) {
                        bool objNeedsRefresh = false;
                        std::string propToDelete = "";

                        if (ImGui::Button("Focus")) triggerFocus = true;
                        ImGui::SameLine();

                        bool isHidden = selection.hiddenObjects.count(obj.guid) > 0;
                        if (ImGui::Button(isHidden ? "Show" : "Hide")) {
                            if (isHidden) {
                                selection.hiddenObjects.erase(obj.guid);
                            } else {
                                selection.hiddenObjects.insert(obj.guid);
                                objToDeselect = obj.guid;
                            }
                            selection.hiddenStateChanged = true;
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Delete Entity")) objToDeleteEntirely = obj.guid;

                        if (ImGui::BeginTable("PropTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                            ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch, 0.4f);
                            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.4f);
                            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 65.0f);

                            std::vector<std::string> allKeys;
                            for (auto& [k, v] : obj.properties) allKeys.push_back(k);
                            for (auto& k : selection.deletedProperties[obj.guid]) {
                                if (std::find(allKeys.begin(), allKeys.end(), k) == allKeys.end()) allKeys.push_back(k);
                            }

                            for (auto& key : allKeys) {
                                if (!locFilter.empty() && !icontains(key, locFilter) && !icontains(obj.properties[key].value, locFilter)) continue;

                                bool isPropDeleted = selection.deletedProperties[obj.guid].count(key) > 0;
                                bool isPropEdited  = !isPropDeleted && selection.originalProperties[obj.guid].count(key) > 0 && selection.originalProperties[obj.guid][key] != obj.properties[key].value;

                                ImGui::TableNextRow();

                                if (isPropDeleted) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                                ImGui::TableSetColumnIndex(0); ImGui::TextWrapped("%s", key.c_str());
                                ImGui::TableSetColumnIndex(1);

                                ImGui::PushID((obj.guid + key).c_str());
                                if (selection.activeEditGuid == obj.guid && selection.activeEditKey == key) {
                                    if (selection.focusEditField) { ImGui::SetKeyboardFocusHere(); selection.focusEditField = false; }
                                    if (ImGui::InputText("##edit", selection.editBuffer, sizeof(selection.editBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
                                        if (selection.originalProperties[obj.guid].find(key) == selection.originalProperties[obj.guid].end()) {
                                            selection.originalProperties[obj.guid][key] = obj.properties[key].value;
                                        }
                                        document->UpdateElementProperty(obj.guid, key, selection.editBuffer);
                                        selection.activeEditGuid = "";
                                        objNeedsRefresh = true;
                                    }

                                    ImGui::TableSetColumnIndex(2);
                                    if (ImGui::Button(ICON_FA_CHECK, sqBtn)) {
                                        if (selection.originalProperties[obj.guid].find(key) == selection.originalProperties[obj.guid].end()) {
                                            selection.originalProperties[obj.guid][key] = obj.properties[key].value;
                                        }
                                        document->UpdateElementProperty(obj.guid, key, selection.editBuffer);
                                        selection.activeEditGuid = "";
                                        objNeedsRefresh = true;
                                    }
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Confirm change");
                                    ImGui::SameLine();
                                    if (ImGui::Button(ICON_FA_BAN, sqBtn)) { selection.activeEditGuid = ""; }
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cancel change");

                                } else {
                                    if (isPropDeleted) ImGui::TextWrapped("<Deleted>");
                                    else ImGui::TextWrapped("%s", obj.properties[key].value.c_str());

                                    ImGui::TableSetColumnIndex(2);
                                    if (isPropEdited || isPropDeleted) {
                                        if (ImGui::Button(ICON_FA_UNDO, sqBtn)) {
                                            std::string orig = selection.originalProperties[obj.guid][key];
                                            document->UpdateElementProperty(obj.guid, key, orig);
                                            selection.deletedProperties[obj.guid].erase(key);
                                            selection.originalProperties[obj.guid].erase(key);
                                            objNeedsRefresh = true;
                                        }
                                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Undo edit/delete");
                                        if (!isPropDeleted) ImGui::SameLine();
                                    }

                                    if (!isPropDeleted) {
                                        if (!isPropEdited) {
                                            if (ImGui::Button(ICON_FA_EDIT, sqBtn)) {
                                                selection.activeEditGuid = obj.guid;
                                                selection.activeEditKey = key;
                                                strncpy(selection.editBuffer, obj.properties[key].value.c_str(), sizeof(selection.editBuffer));
                                                selection.focusEditField = true;
                                            }
                                            ImGui::SameLine();
                                        }
                                        if (ImGui::Button(ICON_FA_TRASH, sqBtn)) { propToDelete = key; }
                                    }
                                }
                                ImGui::PopID();
                                if (isPropDeleted) ImGui::PopStyleColor();
                            }
                            ImGui::EndTable();
                        }

                        if (!propToDelete.empty()) {
                            if (selection.originalProperties[obj.guid].find(propToDelete) == selection.originalProperties[obj.guid].end()) {
                                selection.originalProperties[obj.guid][propToDelete] = obj.properties[propToDelete].value;
                            }
                            selection.deletedProperties[obj.guid].insert(propToDelete);
                            document->UpdateElementProperty(obj.guid, propToDelete, "");
                            obj.properties.erase(propToDelete);
                            objNeedsRefresh = true;
                        }

                        if (objNeedsRefresh) {
                            obj.properties = document->GetElementProperties(obj.guid);
                            if (obj.properties.count("Name") && !obj.properties["Name"].value.empty()) {
                                selection.cachedNames[obj.guid] = obj.properties["Name"].value;
                            }
                        }
                        ImGui::TreePop();
                    }
                }

                if (!objToDeselect.empty()) {
                    selection.objects.erase(std::remove_if(selection.objects.begin(), selection.objects.end(),
                                                           [&](const SelectedObject& o) { return o.guid == objToDeselect; }), selection.objects.end());
                }

                if (!objToDeleteEntirely.empty()) {
                    selection.deletedObjects.insert(objToDeleteEntirely);
                    selection.hiddenObjects.insert(objToDeleteEntirely);
                    selection.objects.erase(std::remove_if(selection.objects.begin(), selection.objects.end(),
                                                           [&](const SelectedObject& o) { return o.guid == objToDeleteEntirely; }), selection.objects.end());
                    selection.hiddenStateChanged = true;
                }
            }
        }
        ImGui::End();

        if (ImGui::IsKeyPressed(ImGuiKey_Escape) && !editingActiveAtStartOfFrame) {
            selection.objects.clear();
        }

        // =====================================================================
        // RIGHT CLICK CONTEXT MENU (With Drag Cancellation)
        // =====================================================================
        bool isHoveringUI = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) || ImGui::IsAnyItemHovered();

        // Use ImGui::GetIO() to check how far the mouse actually traveled during the click
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) &&
            (ImGui::GetIO().MouseDragMaxDistanceSqr[ImGuiMouseButton_Right] < 25.0f) &&
            !isHoveringUI)
        {
            ImGui::OpenPopup("##3DContextMenu");
        }

        if (ImGui::BeginPopup("##3DContextMenu")) {
            if (!selection.objects.empty()) {
                ImGui::TextDisabled("%zu Elements Selected", selection.objects.size());
                ImGui::Separator();

                if (ImGui::MenuItem(ICON_FA_MOUSE_POINTER "  Focus Selection")) { triggerFocus = true; }

                bool anyVisible = false;
                for (const auto& obj : selection.objects) {
                    if (selection.hiddenObjects.count(obj.guid) == 0) { anyVisible = true; break; }
                }

                if (ImGui::MenuItem(anyVisible ? ICON_FA_EYE "  Hide Selection" : ICON_FA_EYE "  Show Selection")) {
                    for (const auto& obj : selection.objects) {
                        if (anyVisible) selection.hiddenObjects.insert(obj.guid);
                        else selection.hiddenObjects.erase(obj.guid);
                    }
                    selection.hiddenStateChanged = true;
                }

                if (ImGui::MenuItem(ICON_FA_TRASH "  Delete Selection")) {
                    for (const auto& obj : selection.objects) {
                        selection.deletedObjects.insert(obj.guid);
                        selection.hiddenObjects.insert(obj.guid);
                    }
                    selection.objects.clear();
                    selection.hiddenStateChanged = true;
                }
            } else {
                ImGui::TextDisabled("Select elements to view actions.");
            }
            ImGui::EndPopup();
        }

        ImGui::PopStyleVar(); // Pop ButtonTextAlign
        ImGui::Render();
    }
} // namespace BimCore
