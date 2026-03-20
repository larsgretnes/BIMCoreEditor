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

        // ---------------------------------------------------------------------
        // FLY MODE HUD OVERLAY (Always renders if active)
        // ---------------------------------------------------------------------
        if (isFlightMode) {
            io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);

            ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + 20.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.65f); // Transparent background
            ImGui::Begin("FlyModeOverlay", nullptr, 
                ImGuiWindowFlags_NoDecoration | 
                ImGuiWindowFlags_AlwaysAutoResize | 
                ImGuiWindowFlags_NoSavedSettings | 
                ImGuiWindowFlags_NoFocusOnAppearing | 
                ImGuiWindowFlags_NoNav | 
                ImGuiWindowFlags_NoMove);
                
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), ICON_FA_ARROWS_ALT "  FLY MODE ACTIVE");
            ImGui::Text("Press TAB to unlock cursor");
            ImGui::End();
        }

        if (!selection.showUI) {
            ImGui::Render();
            return;
        }

        bool editingActiveAtStartOfFrame = !selection.activeEditGuid.empty();
        bool isActivelyLoading = selection.loadState && !selection.loadState->isLoaded.load() && selection.loadState->progress.load() > 0.0f;

        const float leftPanelWidth = 400.0f;
        const float rightPanelWidth = 450.0f;
        const float statsPanelHeight = 150.0f;
        const float mainPanelHeight = viewport->WorkSize.y - statsPanelHeight;

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

        if (ImGui::Button(ICON_FA_FOLDER_OPEN)) selection.triggerLoad = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Open");
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_SAVE)) selection.triggerSave = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save As");

        float rightButtonGroupWidth = 100.0f;
        float cursorX = ImGui::GetWindowContentRegionMax().x - rightButtonGroupWidth;
        if (cursorX > ImGui::GetCursorPosX()) ImGui::SameLine(cursorX);
        else ImGui::SameLine();

        auto drawToolBtn = [&](InteractionTool tool, const char* icon, const char* id) {
            if (selection.activeTool == tool) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button(icon)) selection.activeTool = tool;
            if (selection.activeTool == tool) ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", id);
        };

        drawToolBtn(InteractionTool::Select, ICON_FA_MOUSE_POINTER, "Select (1)"); ImGui::SameLine();
        drawToolBtn(InteractionTool::Pan, ICON_FA_ARROWS_ALT, "Pan (2)"); ImGui::SameLine();
        drawToolBtn(InteractionTool::Orbit, ICON_FA_SYNC, "Orbit (3)");

        ImGui::Spacing();
        if (ImGui::Button(ICON_FA_TIMES_CIRCLE)) { selection.objects.clear(); }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear selected");
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_EYE)) { selection.hiddenObjects.clear(); selection.hiddenStateChanged = true; }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show all");
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_HISTORY)) { ImGui::OpenPopup("Reset Model"); }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset");

        if (ImGui::BeginPopupModal("Reset Model", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("This will reset all items back to original.\nAre you sure?");
            ImGui::Separator();

            if (ImGui::Button("OK", ImVec2(120, 0))) {
                if (document) {
                    for (auto& [guid, props] : selection.originalProperties) {
                        for (auto& [k, v] : props) document->UpdateElementProperty(guid, k, v);
                    }
                }
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
        ImGui::SliderFloat("Explode", &selection.explodeFactor, 0.0f, configMaxExplode);
        if (ImGui::IsItemActive()) selection.updateGeometry = true;

        if (ImGui::CollapsingHeader("Clipping Planes")) {
            ImGui::Checkbox("X", &selection.showPlaneX); ImGui::SameLine(); ImGui::SliderFloat("##x", &selection.clipX, -100.0f, 100.0f);
            ImGui::Checkbox("Y", &selection.showPlaneY); ImGui::SameLine(); ImGui::SliderFloat("##y", &selection.clipY, -100.0f, 100.0f);
            ImGui::Checkbox("Z", &selection.showPlaneZ); ImGui::SameLine(); ImGui::SliderFloat("##z", &selection.clipZ, -100.0f, 100.0f);
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

                    ImGuiListClipper clipper;
                    clipper.Begin(static_cast<int>(selection.searchResults.size()));
                    while (clipper.Step()) {
                        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
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

                            if (ImGui::Selectable(label.c_str(), isSelected)) {
                                handleShiftSelection(i, res.subMeshIndex, "SEARCH_RES", searchMeshIndices, subMeshes);
                            }
                            if (isSelected && triggerFocus) ImGui::SetScrollHereY(0.5f);
                        }
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

                    if (ImGui::TreeNodeEx(type.c_str(), 0, "%s (%zu)%s", type.c_str(), indices.size(), extraTags.c_str())) {

                        ImGuiListClipper clipper;
                        clipper.Begin(static_cast<int>(indices.size()));
                        while (clipper.Step()) {
                            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                                const auto& sub = subMeshes[indices[i]];

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

                                if (ImGui::Selectable(label.c_str(), isSelected) && !isDeleted) {
                                    handleShiftSelection(i, indices[i], type, indices, subMeshes);
                                }

                                if (isSelected && triggerFocus) ImGui::SetScrollHereY(0.5f);
                                if (isDeleted || isHidden) ImGui::PopStyleColor();

                                if (isDeleted) {
                                    ImVec2 min = ImGui::GetItemRectMin();
                                    ImVec2 max = ImGui::GetItemRectMax();
                                    float midY = (min.y + max.y) * 0.5f;
                                    ImGui::GetWindowDrawList()->AddLine(ImVec2(min.x, midY), ImVec2(max.x, midY), IM_COL32(200, 50, 50, 255), 1.5f);
                                }

                                if (isDeleted) {
                                    ImGui::SameLine();
                                    ImGui::PushID((sub.guid + "_undo").c_str());
                                    if (ImGui::Button(ICON_FA_UNDO)) {
                                        selection.deletedObjects.erase(sub.guid);
                                        selection.hiddenObjects.erase(sub.guid);
                                        selection.hiddenStateChanged = true;
                                    }
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Undo delete");
                                    ImGui::PopID();
                                }
                            }
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
            ImGui::InputTextWithHint("##locSearch", ICON_FA_SEARCH " Filter Properties...", selection.localSearchBuf, sizeof(selection.localSearchBuf));
            std::string locFilter = selection.localSearchBuf;
            ImGui::Separator();

            std::string objToDeleteEntirely = "";
            std::string objToDeselect = "";

            ImGuiTreeNodeFlags nodeFlags = (selection.objects.size() == 1 || !locFilter.empty()) ? ImGuiTreeNodeFlags_DefaultOpen : 0;

            for (auto& obj : selection.objects) {
                std::string shortGuid = obj.guid.length() >= 8 ? obj.guid.substr(obj.guid.length() - 8) : obj.guid;

                std::string headerName = obj.type;
                if (selection.cachedNames.count(obj.guid)) headerName = selection.cachedNames[obj.guid];

                std::string treeId = obj.guid + "_tree";
                if (ImGui::TreeNodeEx(treeId.c_str(), nodeFlags, "%s [%s]", headerName.c_str(), shortGuid.c_str())) {
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
                                if (ImGui::Button(ICON_FA_CHECK)) {
                                    if (selection.originalProperties[obj.guid].find(key) == selection.originalProperties[obj.guid].end()) {
                                        selection.originalProperties[obj.guid][key] = obj.properties[key].value;
                                    }
                                    document->UpdateElementProperty(obj.guid, key, selection.editBuffer);
                                    selection.activeEditGuid = "";
                                    objNeedsRefresh = true;
                                }
                                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Confirm change");
                                ImGui::SameLine();
                                if (ImGui::Button(ICON_FA_BAN)) { selection.activeEditGuid = ""; }
                                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cancel change");

                            } else {
                                if (isPropDeleted) ImGui::TextWrapped("<Deleted>");
                                else ImGui::TextWrapped("%s", obj.properties[key].value.c_str());

                                ImGui::TableSetColumnIndex(2);
                                if (isPropEdited || isPropDeleted) {
                                    if (ImGui::Button(ICON_FA_UNDO)) {
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
                                        if (ImGui::Button(ICON_FA_EDIT)) {
                                            selection.activeEditGuid = obj.guid;
                                            selection.activeEditKey = key;
                                            strncpy(selection.editBuffer, obj.properties[key].value.c_str(), sizeof(selection.editBuffer));
                                            selection.focusEditField = true;
                                        }
                                        ImGui::SameLine();
                                    }
                                    if (ImGui::Button(ICON_FA_TRASH)) { propToDelete = key; }
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
        ImGui::End();

        if (ImGui::IsKeyPressed(ImGuiKey_Escape) && !editingActiveAtStartOfFrame) {
            selection.objects.clear();
        }

        ImGui::Render();
    }
} // namespace BimCore