// =============================================================================
// BimCore/apps/editor/ui/UIPropertiesPanel.cpp
// =============================================================================
#include "UIPropertiesPanel.h"
#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include "platform/portable-file-dialogs.h"

#define ICON_FA_SEARCH        "\xef\x80\x82"
#define ICON_FA_DOWNLOAD      "\xef\x80\x99"
#define ICON_FA_CHECK         "\xef\x80\x8c"
#define ICON_FA_BAN           "\xef\x81\x9e"
#define ICON_FA_EDIT          "\xef\x8c\x83"
#define ICON_FA_TRASH         "\xef\x80\x8d"
#define ICON_FA_UNDO          "\xef\x80\x9e"

namespace BimCore {

    static bool icontains(const std::string& str, const std::string& query) {
        if (query.empty()) return true;
        auto it = std::search(str.begin(), str.end(), query.begin(), query.end(),
                              [](unsigned char ch1, unsigned char ch2) { return std::tolower(ch1) == std::tolower(ch2); });
        return it != str.end();
    }

    void UIPropertiesPanel::Render(SelectionState& state, std::shared_ptr<BimDocument> document, bool& triggerFocus) {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float rightPanelWidth = 450.0f;

        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x, viewport->WorkPos.y), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        ImGui::SetNextWindowSizeConstraints(ImVec2(300, viewport->WorkSize.y), ImVec2(viewport->WorkSize.x / 2.0f, viewport->WorkSize.y));
        ImGui::SetNextWindowSize(ImVec2(rightPanelWidth, viewport->WorkSize.y), ImGuiCond_FirstUseEver);

        ImGui::Begin("Properties", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

        if (state.objects.empty()) {
            ImGui::TextDisabled("Select an element to view properties.");
            ImGui::End();
            return;
        }

        float exportBtnWidth = 110.0f;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - exportBtnWidth - ImGui::GetStyle().ItemSpacing.x);
        ImGui::InputTextWithHint("##locSearch", ICON_FA_SEARCH " Filter Properties...", state.localSearchBuf, sizeof(state.localSearchBuf));

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_DOWNLOAD " Export CSV", ImVec2(exportBtnWidth, 0))) {
            auto fd = pfd::save_file("Export Selection", "Selection_Properties.csv", { "CSV Files", "*.csv" });
            std::string path = fd.result();
            if (!path.empty()) {
                std::ofstream out(path);
                out << "GUID,Type,Property,Value\n";
                for (const auto& obj : state.objects) {
                    for (const auto& [k, v] : obj.properties) {
                        out << obj.guid << "," << obj.type << ",\"" << k << "\",\"" << v.value << "\"\n";
                    }
                }
            }
        }
        std::string locFilter = state.localSearchBuf;
        ImGui::Separator();

        bool deleteAll = false;
        ImVec2 sqBtn(ImGui::GetFrameHeight(), ImGui::GetFrameHeight());

        // --- GLOBAL ACTIONS ROW (Centered & Colored) ---
        if (state.objects.size() > 1) {
            bool anyVisible = false;
            for (const auto& obj : state.objects) {
                if (state.hiddenObjects.count(obj.guid) == 0) { anyVisible = true; break; }
            }
            const char* hideLabel = anyVisible ? "Hide All" : "Show All";

            float btnW1 = ImGui::CalcTextSize("Focus All").x + ImGui::GetStyle().FramePadding.x * 2.0f;
            float btnW2 = ImGui::CalcTextSize(hideLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
            float btnW3 = ImGui::CalcTextSize("Delete All").x + ImGui::GetStyle().FramePadding.x * 2.0f;
            float spacing = ImGui::GetStyle().ItemSpacing.x;
            float totalW = btnW1 + btnW2 + btnW3 + (spacing * 2.0f);

            float cx = (ImGui::GetWindowContentRegionMax().x - totalW) * 0.5f;
            if (cx > 0) ImGui::SetCursorPosX(cx);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.3f, 0.1f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.45f, 0.35f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.4f, 0.2f, 1.0f));

            if (ImGui::Button("Focus All")) { triggerFocus = true; }
            ImGui::SameLine();

            if (ImGui::Button(hideLabel)) {
                for (const auto& obj : state.objects) {
                    if (anyVisible) state.hiddenObjects.insert(obj.guid);
                    else state.hiddenObjects.erase(obj.guid);
                }
                state.hiddenStateChanged = true;
            }
            ImGui::SameLine();

            if (ImGui::Button("Delete All")) { deleteAll = true; }

            ImGui::PopStyleColor(3);
            ImGui::Separator();
        }

        if (deleteAll) {
            for (const auto& obj : state.objects) {
                state.deletedObjects.insert(obj.guid);
                state.hiddenObjects.insert(obj.guid);
            }
            state.objects.clear();
            state.hiddenStateChanged = true;
            state.selectionChanged = true;
        } else {
            std::string objToDeleteEntirely = "";
            std::string objToDeselect = "";
            bool globalRefreshNeeded = false;

            if (state.objects.size() > 1) {
                DrawSharedPropertyTable(state, document, locFilter, sqBtn, globalRefreshNeeded);
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
            }

            if (globalRefreshNeeded) {
                for (auto& obj : state.objects) {
                    obj.properties = document->GetElementProperties(obj.guid);
                    if (obj.properties.count("Name") && !obj.properties["Name"].value.empty()) {
                        state.cachedNames[obj.guid] = obj.properties["Name"].value;
                    }
                }
            }

            ImGui::TextDisabled("Selected Elements");
            ImGui::Spacing();
            ImGui::BeginChild("SelectionList", ImVec2(0, 0), true);

            for (auto& obj : state.objects) {
                std::string shortGuid = obj.guid.length() >= 8 ? obj.guid.substr(obj.guid.length() - 8) : obj.guid;
                std::string headerName = obj.type;
                if (state.cachedNames.count(obj.guid)) headerName = state.cachedNames[obj.guid];

                std::string treeId = obj.guid + "_tree";

                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.3f, 0.45f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.35f, 0.5f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.25f, 0.4f, 0.55f, 1.0f));

                ImGuiTreeNodeFlags nodeFlags = (state.objects.size() == 1 || !locFilter.empty()) ? ImGuiTreeNodeFlags_DefaultOpen : 0;
                bool isObjOpen = ImGui::TreeNodeEx(treeId.c_str(), nodeFlags | ImGuiTreeNodeFlags_Framed, "%s [%s]", headerName.c_str(), shortGuid.c_str());
                ImGui::PopStyleColor(3);

                if (isObjOpen) {
                    bool objNeedsRefresh = false;
                    std::string propToDelete = "";

                    if (ImGui::Button("Focus")) triggerFocus = true;
                    ImGui::SameLine();

                    bool isHidden = state.hiddenObjects.count(obj.guid) > 0;
                    if (ImGui::Button(isHidden ? "Show" : "Hide")) {
                        if (isHidden) {
                            state.hiddenObjects.erase(obj.guid);
                        } else {
                            state.hiddenObjects.insert(obj.guid);
                            objToDeselect = obj.guid;
                        }
                        state.hiddenStateChanged = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Delete Entity")) objToDeleteEntirely = obj.guid;

                    DrawPropertyTable(state, obj, document, locFilter, sqBtn, objNeedsRefresh, propToDelete);
                    ImGui::TreePop();
                }
            }

            ImGui::EndChild();

            if (!objToDeselect.empty()) {
                state.objects.erase(std::remove_if(state.objects.begin(), state.objects.end(),
                                                   [&](const SelectedObject& o) { return o.guid == objToDeselect; }), state.objects.end());
                state.selectionChanged = true;
            }

            if (!objToDeleteEntirely.empty()) {
                state.deletedObjects.insert(objToDeleteEntirely);
                state.hiddenObjects.insert(objToDeleteEntirely);
                state.objects.erase(std::remove_if(state.objects.begin(), state.objects.end(),
                                                   [&](const SelectedObject& o) { return o.guid == objToDeleteEntirely; }), state.objects.end());
                state.hiddenStateChanged = true;
                state.selectionChanged = true;
            }
        }
        ImGui::End();
    }

    void UIPropertiesPanel::DrawSharedPropertyTable(SelectionState& state, std::shared_ptr<BimDocument> document, const std::string& locFilter, const ImVec2& sqBtn, bool& globalRefreshNeeded) {
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
            for (const auto& obj : state.objects) {
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
                        if (state.activeEditGuid == "SHARED" && state.activeEditKey == key) {
                            if (state.focusEditField) { ImGui::SetKeyboardFocusHere(); state.focusEditField = false; }

                            bool enterPressed = ImGui::InputText("##edit", state.editBuffer, sizeof(state.editBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

                            ImGui::TableSetColumnIndex(2);
                            bool confirmPressed = ImGui::Button(ICON_FA_CHECK, sqBtn);
                            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Apply to all selected");

                            ImGui::SameLine();
                            if (ImGui::Button(ICON_FA_BAN, sqBtn)) { state.activeEditGuid = ""; }
                            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cancel");

                            if (enterPressed || confirmPressed) {
                                for (auto& obj : state.objects) {
                                    if (state.originalProperties[obj.guid].find(key) == state.originalProperties[obj.guid].end()) {
                                        state.originalProperties[obj.guid][key] = obj.properties[key].value;
                                    }
                                    document->UpdateElementProperty(obj.guid, key, state.editBuffer);
                                }
                                state.activeEditGuid = "";
                                globalRefreshNeeded = true;
                            }
                        } else {
                            if (isMulti[key]) {
                                ImGui::TextDisabled("<Multiple Values>");
                            } else {
                                ImGui::TextWrapped("%s", sharedVals[key].c_str());
                                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                                    state.activeEditGuid = "SHARED";
                                    state.activeEditKey = key;
                                    strncpy(state.editBuffer, sharedVals[key].c_str(), sizeof(state.editBuffer));
                                    state.focusEditField = true;
                                }
                            }

                            ImGui::TableSetColumnIndex(2);
                            if (ImGui::Button(ICON_FA_EDIT, sqBtn)) {
                                state.activeEditGuid = "SHARED";
                                state.activeEditKey = key;
                                strncpy(state.editBuffer, isMulti[key] ? "" : sharedVals[key].c_str(), sizeof(state.editBuffer));
                                state.focusEditField = true;
                            }
                            ImGui::SameLine();
                            if (ImGui::Button(ICON_FA_TRASH, sqBtn)) {
                                for (auto& obj : state.objects) {
                                    if (state.originalProperties[obj.guid].find(key) == state.originalProperties[obj.guid].end()) {
                                        state.originalProperties[obj.guid][key] = obj.properties[key].value;
                                    }
                                    state.deletedProperties[obj.guid].insert(key);
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
    }

    void UIPropertiesPanel::DrawPropertyTable(SelectionState& state, SelectedObject& obj, std::shared_ptr<BimDocument> document, const std::string& locFilter, const ImVec2& sqBtn, bool& objNeedsRefresh, std::string& propToDelete) {
        if (ImGui::BeginTable("PropTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch, 0.4f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.4f);
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 65.0f);

            std::vector<std::string> allKeys;
            for (auto& [k, v] : obj.properties) allKeys.push_back(k);
            for (auto& k : state.deletedProperties[obj.guid]) {
                if (std::find(allKeys.begin(), allKeys.end(), k) == allKeys.end()) allKeys.push_back(k);
            }

            for (auto& key : allKeys) {
                if (!locFilter.empty() && !icontains(key, locFilter) && !icontains(obj.properties[key].value, locFilter)) continue;

                bool isPropDeleted = state.deletedProperties[obj.guid].count(key) > 0;
                bool isPropEdited  = !isPropDeleted && state.originalProperties[obj.guid].count(key) > 0 && state.originalProperties[obj.guid][key] != obj.properties[key].value;

                ImGui::TableNextRow();

                if (isPropDeleted) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                ImGui::TableSetColumnIndex(0); ImGui::TextWrapped("%s", key.c_str());
                ImGui::TableSetColumnIndex(1);

                ImGui::PushID((obj.guid + key).c_str());
                if (state.activeEditGuid == obj.guid && state.activeEditKey == key) {
                    if (state.focusEditField) { ImGui::SetKeyboardFocusHere(); state.focusEditField = false; }
                    if (ImGui::InputText("##edit", state.editBuffer, sizeof(state.editBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
                        if (state.originalProperties[obj.guid].find(key) == state.originalProperties[obj.guid].end()) {
                            state.originalProperties[obj.guid][key] = obj.properties[key].value;
                        }
                        document->UpdateElementProperty(obj.guid, key, state.editBuffer);
                        state.activeEditGuid = "";
                        objNeedsRefresh = true;
                    }

                    ImGui::TableSetColumnIndex(2);
                    if (ImGui::Button(ICON_FA_CHECK, sqBtn)) {
                        if (state.originalProperties[obj.guid].find(key) == state.originalProperties[obj.guid].end()) {
                            state.originalProperties[obj.guid][key] = obj.properties[key].value;
                        }
                        document->UpdateElementProperty(obj.guid, key, state.editBuffer);
                        state.activeEditGuid = "";
                        objNeedsRefresh = true;
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Confirm change");
                    ImGui::SameLine();
                    if (ImGui::Button(ICON_FA_BAN, sqBtn)) { state.activeEditGuid = ""; }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cancel change");

                } else {
                    if (isPropDeleted) {
                        ImGui::TextWrapped("<Deleted>");
                    } else {
                        ImGui::TextWrapped("%s", obj.properties[key].value.c_str());
                        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                            state.activeEditGuid = obj.guid;
                            state.activeEditKey = key;
                            strncpy(state.editBuffer, obj.properties[key].value.c_str(), sizeof(state.editBuffer));
                            state.focusEditField = true;
                        }
                    }

                    ImGui::TableSetColumnIndex(2);
                    if (isPropEdited || isPropDeleted) {
                        if (ImGui::Button(ICON_FA_UNDO, sqBtn)) {
                            std::string orig = state.originalProperties[obj.guid][key];
                            document->UpdateElementProperty(obj.guid, key, orig);
                            state.deletedProperties[obj.guid].erase(key);
                            state.originalProperties[obj.guid].erase(key);
                            objNeedsRefresh = true;
                        }
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Undo edit/delete");
                        if (!isPropDeleted) ImGui::SameLine();
                    }

                    if (!isPropDeleted) {
                        if (!isPropEdited) {
                            if (ImGui::Button(ICON_FA_EDIT, sqBtn)) {
                                state.activeEditGuid = obj.guid;
                                state.activeEditKey = key;
                                strncpy(state.editBuffer, obj.properties[key].value.c_str(), sizeof(state.editBuffer));
                                state.focusEditField = true;
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
            if (state.originalProperties[obj.guid].find(propToDelete) == state.originalProperties[obj.guid].end()) {
                state.originalProperties[obj.guid][propToDelete] = obj.properties[propToDelete].value;
            }
            state.deletedProperties[obj.guid].insert(propToDelete);
            document->UpdateElementProperty(obj.guid, propToDelete, "");
            obj.properties.erase(propToDelete);
            objNeedsRefresh = true;
        }
    }

} // namespace BimCore
