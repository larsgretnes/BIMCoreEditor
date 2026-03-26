// =============================================================================
// BimCore/apps/editor/ui/UISearchPanel.cpp
// =============================================================================
#include "UISearchPanel.h"
#include "UIModelTree.h" // For Context Menu
#include <imgui.h>
#include <algorithm>
#include <thread>
#include <cctype>
#include <fstream>
#include "platform/portable-file-dialogs.h"

#define ICON_FA_SEARCH   "\xef\x80\x82"
#define ICON_FA_DOWNLOAD "\xef\x80\x99"

namespace BimCore {

    bool UISearchPanel::IContains(const std::string& str, const std::string& query) {
        if (query.empty()) return true;
        auto it = std::search(str.begin(), str.end(), query.begin(), query.end(),
                              [](unsigned char ch1, unsigned char ch2) { return std::tolower(ch1) == std::tolower(ch2); });
        return it != str.end();
    }

    void UISearchPanel::Render(SelectionState& state, std::vector<std::shared_ptr<SceneModel>>& documents, bool& triggerFocus) {
        bool enterPressed = ImGui::InputTextWithHint("##globSearch", ICON_FA_SEARCH " Search All Models...", state.globalSearchBuf, sizeof(state.globalSearchBuf), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("Search") || enterPressed) {
            std::string query = state.globalSearchBuf;
            if (!query.empty() && !state.isSearching.load()) {
                state.isSearchActive = true;
                state.isSearching.store(true);

                std::thread([&state, query, documents]() {
                    std::vector<SearchResult> results;
                    uint32_t flatIndex = 0;
                    
                    for (auto& doc : documents) {
                        const auto& subMeshes = doc->GetGeometry().subMeshes;
                        for (uint32_t i = 0; i < subMeshes.size(); ++i) {
                            const auto& sub = subMeshes[i];
                            uint32_t currentFlatIndex = flatIndex + i;

                            if (!state.showOpeningsAndSpaces && (sub.type == "IfcOpeningElement" || sub.type == "IfcSpace")) continue;

                            std::string nameSearchTarget = sub.type;
                            if (state.cachedNames.count(sub.guid)) nameSearchTarget = state.cachedNames[sub.guid];

                            if (IContains(nameSearchTarget, query)) {
                                results.push_back({currentFlatIndex, "Type/Name", "Name", nameSearchTarget});
                                continue;
                            }
                            if (IContains(sub.guid, query)) {
                                results.push_back({currentFlatIndex, "GUID", "GUID", sub.guid});
                                continue;
                            }
                            auto props = doc->GetElementProperties(sub.guid);
                            for (const auto& [pk, pv] : props) {
                                if (IContains(pk, query) || IContains(pv.value, query)) {
                                    results.push_back({currentFlatIndex, "Property", pk, pv.value});
                                    break;
                                }
                            }
                        }
                        flatIndex += static_cast<uint32_t>(subMeshes.size());
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

        if (state.isSearchActive) {
            if (state.isSearching.load()) {
                ImGui::TextDisabled("Searching the scene...");
            } else {
                std::lock_guard<std::mutex> lock(state.searchMutex);

                auto getSubMesh = [&](uint32_t flatIndex, std::shared_ptr<SceneModel>& outDoc, const RenderSubMesh*& outSub, uint32_t& outLocalIdx) {
                    uint32_t offset = 0;
                    for (auto& doc : documents) {
                        uint32_t count = static_cast<uint32_t>(doc->GetGeometry().subMeshes.size());
                        if (flatIndex < offset + count) {
                            outDoc = doc;
                            outLocalIdx = flatIndex - offset;
                            outSub = &doc->GetGeometry().subMeshes[outLocalIdx];
                            return;
                        }
                        offset += count;
                    }
                };

                if (ImGui::Button(ICON_FA_DOWNLOAD " Export CSV")) {
                    auto fd = pfd::save_file("Export Results", "Search_Results.csv", { "CSV Files", "*.csv" });
                    std::string path = fd.result();
                    if (!path.empty()) {
                        std::ofstream out(path);
                        out << "GUID,Type,Match Type,Match Key,Match Value\n";
                        for (auto& res : state.searchResults) {
                            std::shared_ptr<SceneModel> doc; const RenderSubMesh* sub = nullptr; uint32_t localIdx = 0;
                            getSubMesh(res.subMeshIndex, doc, sub, localIdx);
                            if (sub) {
                                out << sub->guid << "," << sub->type << "," << res.matchType << "," << "\"" << res.matchKey << "\",\"" << res.matchValue << "\"\n";
                            }
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
                            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);

                            const auto& res = state.searchResults[i];
                            std::shared_ptr<SceneModel> doc; const RenderSubMesh* sub = nullptr; uint32_t localIdx = 0;
                            getSubMesh(res.subMeshIndex, doc, sub, localIdx);
                            
                            if (!doc || !sub) continue;

                            if (state.cachedNames.find(sub->guid) == state.cachedNames.end()) {
                                auto props = doc->GetElementProperties(sub->guid);
                                state.cachedNames[sub->guid] = (props.count("Name") && !props["Name"].value.empty()) ? props["Name"].value : sub->type;
                            }

                            std::string shortGuid = sub->guid.length() >= 8 ? sub->guid.substr(sub->guid.length() - 8) : sub->guid;
                            std::string snippet = res.matchType == "Property" ? (res.matchKey + ": " + res.matchValue) : res.matchValue;
                            std::string label = state.cachedNames[sub->guid] + " [" + shortGuid + "] - " + snippet + "###" + sub->guid;

                            bool isSelected = std::any_of(state.objects.begin(), state.objects.end(), [&](const SelectedObject& o) { return o.guid == sub->guid; });

                            ImVec4 hoverColor = isSelected ? ImGui::GetStyleColorVec4(ImGuiCol_Header) : ImVec4(0,0,0,0);
                            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, hoverColor);
                            if (ImGui::Selectable(label.c_str(), isSelected)) {
                                UIModelTree::HandleShiftSelection(state, i, localIdx, "SEARCH_RES", searchMeshIndices, doc);
                            }
                            ImGui::PopStyleColor();

                            if (ImGui::BeginPopupContextItem(("ctx_s_" + sub->guid).c_str())) {
                                UIModelTree::DrawMultiSelectContextMenu(state, sub, doc, triggerFocus);
                                ImGui::EndPopup();
                            }

                            if (isSelected && triggerFocus) ImGui::SetScrollHereY(0.5f);
                        }
                    }
                    ImGui::EndTable();
                }
            }
        }
    }
} // namespace BimCore