// =============================================================================
// BimCore/apps/editor/ui/UIModelTree.cpp
// =============================================================================
#include "UIModelTree.h"
#include <imgui.h>
#include <algorithm>
#include <unordered_set>
#include <filesystem>

#define ICON_FA_EYE "\xef\x81\xae"
#define ICON_FA_UNDO "\xef\x80\x9e"

namespace BimCore {

    void UIModelTree::HandleShiftSelection(SelectionState& state, int visualIdx, uint32_t meshIdx, const std::string& groupName, const std::vector<uint32_t>& currentArray, std::shared_ptr<SceneModel> document) {
        ImGuiIO& io = ImGui::GetIO();
        const auto& subMeshes = document->GetGeometry().subMeshes;

        if (io.KeyShift && state.lastClickedVisualIndex != -1 && state.lastClickedGroup == groupName) {
            if (!io.KeyCtrl) {
                state.objects.clear();
            }
            int start = std::min(state.lastClickedVisualIndex, visualIdx);
            int end = std::max(state.lastClickedVisualIndex, visualIdx);
            for (int j = start; j <= end; ++j) {
                uint32_t targetMeshIdx = currentArray[j];
                const auto& targetSub = subMeshes[targetMeshIdx];
                bool isSel = std::any_of(state.objects.begin(), state.objects.end(), [&](const SelectedObject& o) { return o.guid == targetSub.guid; });
                if (!isSel) {
                    SelectedObject so; 
                    so.guid = targetSub.guid; 
                    so.type = targetSub.type; 
                    so.startIndex = targetSub.globalStartIndex; 
                    so.indexCount = targetSub.indexCount; 
                    so.properties = document->GetElementProperties(targetSub.guid);
                    state.objects.push_back(so);
                }
            }
        } else {
            if (!io.KeyCtrl) {
                state.objects.clear();
            }
            bool isSelected = std::any_of(state.objects.begin(), state.objects.end(), [&](const SelectedObject& o) { return o.guid == subMeshes[meshIdx].guid; });
            if (!isSelected) {
                const auto& targetSub = subMeshes[meshIdx];
                SelectedObject so; 
                so.guid = targetSub.guid; 
                so.type = targetSub.type; 
                so.startIndex = targetSub.globalStartIndex; 
                so.indexCount = targetSub.indexCount; 
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

    void UIModelTree::DrawMultiSelectContextMenu(SelectionState& state, const RenderSubMesh* sub, std::shared_ptr<SceneModel> doc, bool& triggerFocus) {
        bool isSel = std::any_of(state.objects.begin(), state.objects.end(), [&](const SelectedObject& o) { return o.guid == sub->guid; });
        
        if (!isSel) {
            state.objects.clear();
            SelectedObject so; 
            so.guid = sub->guid; 
            so.type = sub->type; 
            so.startIndex = sub->globalStartIndex; 
            so.indexCount = sub->indexCount; 
            so.properties = doc->GetElementProperties(sub->guid);
            state.objects.push_back(so);
            state.selectionChanged = true;
        }

        if (ImGui::MenuItem("Focus Element(s)")) {
            triggerFocus = true;
        }
        ImGui::Separator();
        
        if (ImGui::MenuItem("Hide Element(s)")) {
            for (const auto& o : state.objects) {
                state.hiddenObjects.insert(o.guid);
            }
            state.objects.clear();
            state.hiddenStateChanged = true;
            state.selectionChanged = true;
        }
        if (ImGui::MenuItem("Show Element(s)")) {
            for (const auto& o : state.objects) {
                state.hiddenObjects.erase(o.guid);
            }
            state.hiddenStateChanged = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Delete Element(s)")) {
            for (const auto& o : state.objects) {
                state.deletedObjects.insert(o.guid);
            }
            state.objects.clear();
            state.hiddenStateChanged = true;
            state.selectionChanged = true;
        }
    }

    void UIModelTree::Render(SelectionState& state, std::vector<std::shared_ptr<SceneModel>>& documents, bool& triggerFocus, bool& triggerRebuild) {
        ImGui::Spacing();
        ImGui::Separator();

        ImGui::TextDisabled("Model Tree View");
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 145.0f);
        
        static int s_treeMode = 0; // 0 = Category, 1 = Spatial
        
        if (s_treeMode == 0) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button("Category", ImVec2(65, 0))) s_treeMode = 0;
        if (s_treeMode == 0) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Group items flatly by their IFC Type.");
        
        ImGui::SameLine();
        
        if (s_treeMode == 1) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button("Spatial", ImVec2(65, 0))) s_treeMode = 1;
        if (s_treeMode == 1) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Group items by their physical Location/Storey.");

        ImGui::BeginChild("ModelTree", ImVec2(0, 0), true);
        ImVec2 sqBtn(ImGui::GetFrameHeight(), ImGui::GetFrameHeight());

        for (auto it = documents.begin(); it != documents.end(); ) {
            auto& doc = *it;
            std::string filename = std::filesystem::path(doc->GetFilePath()).filename().string();
            if (doc->IsHidden()) filename += " (Hidden)";

            ImGui::PushID(doc.get());
            bool isFileNodeOpen = ImGui::TreeNodeEx("FileNode", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen, "%s", filename.c_str());

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Select all")) {
                    if (!ImGui::GetIO().KeyCtrl) state.objects.clear();
                    for (const auto& sub : doc->GetGeometry().subMeshes) {
                        if (state.deletedObjects.count(sub.guid) || state.hiddenObjects.count(sub.guid)) continue;
                        bool isSel = std::any_of(state.objects.begin(), state.objects.end(), [&](const SelectedObject& o) { return o.guid == sub.guid; });
                        if (!isSel) {
                            SelectedObject so; 
                            so.guid = sub.guid; 
                            so.type = sub.type; 
                            so.startIndex = sub.globalStartIndex; 
                            so.indexCount = sub.indexCount; 
                            so.properties = doc->GetElementProperties(sub.guid);
                            state.objects.push_back(so);
                        }
                    }
                    state.selectionChanged = true;
                    triggerFocus = true;
                }
                ImGui::Separator();
                if (doc->IsHidden()) {
                    if (ImGui::MenuItem("Show model")) { 
                        doc->SetHidden(false); 
                        state.hiddenStateChanged = true; 
                    }
                } else {
                    if (ImGui::MenuItem("Hide model")) {
                        doc->SetHidden(true);
                        state.objects.erase(std::remove_if(state.objects.begin(), state.objects.end(),
                                                           [&](const SelectedObject& o) {
                                                               for (const auto& sub : doc->GetGeometry().subMeshes) { 
                                                                   if (o.guid == sub.guid) return true; 
                                                               } 
                                                               return false;
                                                           }), state.objects.end());
                        state.hiddenStateChanged = true;
                    }
                }
                ImGui::Separator();
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
                    if (s_treeMode == 0) {
                        // ==========================================
                        // CATEGORY TREE MODE
                        // ==========================================
                        const auto& docGroups = doc->GetUIGroups();
                        const auto& subMeshes = doc->GetGeometry().subMeshes;

                        for (const auto& [type, indices] : docGroups) {
                            if (!state.showOpeningsAndSpaces && (type == "IfcOpeningElement" || type == "IfcSpace")) continue;

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
                            if (groupHasSelected && triggerFocus) {
                                ImGui::SetNextItemOpen(true, ImGuiCond_Always);
                            }
                            if (groupHasHidden) extraTags += " (hidden)";
                            if (groupHasDeleted) extraTags += " (deleted)";
                            if (groupHasEdited) extraTags += " (edited)";

                            std::string selPrefix = groupHasSelected ? "[#] " : "";
                            std::string nodeLabel = selPrefix + type + " (" + std::to_string(indices.size()) + ")" + extraTags;

                            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.3f, 0.45f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.35f, 0.5f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.25f, 0.4f, 0.55f, 1.0f));
                            bool isNodeOpen = ImGui::TreeNodeEx(type.c_str(), ImGuiTreeNodeFlags_Framed, "%s", nodeLabel.c_str());
                            ImGui::PopStyleColor(3);

                            if (ImGui::BeginPopupContextItem()) {
                                if (ImGui::MenuItem("Select category")) {
                                    if (!ImGui::GetIO().KeyCtrl) state.objects.clear();
                                    for (uint32_t idx : indices) {
                                        const auto& sub = subMeshes[idx];
                                        if (state.deletedObjects.count(sub.guid) || state.hiddenObjects.count(sub.guid)) continue;
                                        
                                        bool isSel = std::any_of(state.objects.begin(), state.objects.end(), [&](const SelectedObject& o) { return o.guid == sub.guid; });
                                        if (!isSel) {
                                            SelectedObject so; 
                                            so.guid = sub.guid; 
                                            so.type = sub.type; 
                                            so.startIndex = sub.globalStartIndex; 
                                            so.indexCount = sub.indexCount; 
                                            so.properties = doc->GetElementProperties(sub.guid);
                                            state.objects.push_back(so);
                                        }
                                    }
                                    state.selectionChanged = true;
                                    triggerFocus = true;
                                }
                                ImGui::Separator();
                                if (ImGui::MenuItem("Hide category")) {
                                    for (uint32_t idx : indices) {
                                        state.hiddenObjects.insert(subMeshes[idx].guid);
                                    }
                                    state.hiddenStateChanged = true;
                                }
                                if (ImGui::MenuItem("Show category")) {
                                    for (uint32_t idx : indices) {
                                        state.hiddenObjects.erase(subMeshes[idx].guid);
                                    }
                                    state.hiddenStateChanged = true;
                                }
                                ImGui::EndPopup();
                            }

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

                                            if (!isDeleted && ImGui::BeginPopupContextItem(("ctx_" + sub.guid).c_str())) {
                                                DrawMultiSelectContextMenu(state, &sub, doc, triggerFocus);
                                                ImGui::EndPopup();
                                            }

                                            if (isSelected && triggerFocus) {
                                                ImGui::SetScrollHereY(0.5f);
                                            }
                                            if (isDeleted || isHidden) {
                                                ImGui::PopStyleColor();
                                            }

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
                    } else {
                        // ==========================================
                        // SPATIAL TREE MODE
                        // ==========================================
                        std::unordered_map<std::string, const RenderSubMesh*> geomMap;
                        for (const auto& sub : doc->GetGeometry().subMeshes) {
                            geomMap[sub.guid] = &sub;
                        }

                        std::vector<std::string> rootNodes;
                        std::unordered_set<std::string> processedRoots;
                        
                        for (const auto& sub : doc->GetGeometry().subMeshes) {
                            std::string curr = sub.guid;
                            while (true) {
                                std::string p = doc->GetParent(curr);
                                if (p.empty()) break;
                                curr = p;
                            }
                            if (processedRoots.insert(curr).second) {
                                rootNodes.push_back(curr);
                            }
                        }

                        std::unordered_set<std::string> activeSpatialBranches;
                        for (const auto& obj : state.objects) {
                            std::string curr = obj.guid;
                            while (!curr.empty()) {
                                activeSpatialBranches.insert(curr);
                                curr = doc->GetParent(curr);
                            }
                        }

                        std::vector<std::string> structuralRoots;
                        std::vector<std::string> uncategorizedRoots;

                        for (const auto& root : rootNodes) {
                            auto children = doc->GetChildren(root);
                            bool hasGeom = geomMap.count(root) > 0;
                            
                            if (children.empty() && hasGeom) {
                                uncategorizedRoots.push_back(root);
                            } else {
                                structuralRoots.push_back(root);
                            }
                        }

                        auto drawSpatialNode = [&](const std::string& nodeGuid, auto& self) -> void {
                            auto children = doc->GetChildren(nodeGuid);
                            auto itGeom = geomMap.find(nodeGuid);
                            const RenderSubMesh* sub = (itGeom != geomMap.end()) ? itGeom->second : nullptr;
                            
                            bool hasChildren = !children.empty();
                            bool hasGeom = (sub != nullptr);
                            
                            if (!hasChildren && !hasGeom) return; 

                            if (hasGeom && !state.showOpeningsAndSpaces && (sub->type == "IfcOpeningElement" || sub->type == "IfcSpace")) {
                                if (!hasChildren) return;
                            }
                            
                            std::string name = doc->GetElementNameFast(nodeGuid);
                            std::string shortGuid = nodeGuid.length() >= 8 ? nodeGuid.substr(nodeGuid.length() - 8) : nodeGuid;
                            
                            std::string extraTags = "";
                            bool isHidden = false, isDeleted = false, isEdited = false;
                            
                            if (hasGeom) {
                                isHidden = state.hiddenObjects.count(nodeGuid) > 0;
                                isDeleted = state.deletedObjects.count(nodeGuid) > 0;
                                isEdited = doc->HasModifiedProperties(nodeGuid);
                                if (isEdited && !isDeleted) extraTags += " (Edited)";
                            }

                            bool branchHasSelected = activeSpatialBranches.count(nodeGuid) > 0;
                            if (branchHasSelected && triggerFocus) {
                                ImGui::SetNextItemOpen(true, ImGuiCond_Always);
                            }
                            
                            std::string selPrefix = branchHasSelected ? "[#] " : "";
                            std::string label = selPrefix + name + " [" + shortGuid + "]" + extraTags + "###" + nodeGuid;
                            
                            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
                            if (!hasChildren) {
                                flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                            }
                            
                            bool isSelected = false;
                            if (hasGeom) {
                                isSelected = std::any_of(state.objects.begin(), state.objects.end(), [&](const SelectedObject& o) { return o.guid == nodeGuid; });
                                if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;
                            }
                            
                            if (isDeleted) {
                                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                            } else if (isHidden) {
                                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 1.0f, 1.0f));
                            }
                            
                            bool nodeOpen = ImGui::TreeNodeEx(label.c_str(), flags);
                            
                            if (isDeleted || isHidden) {
                                ImGui::PopStyleColor();
                            }
                            
                            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() && hasGeom && !isDeleted) {
                                if (!ImGui::GetIO().KeyCtrl) state.objects.clear();
                                if (!isSelected) {
                                    SelectedObject so; 
                                    so.guid = sub->guid; 
                                    so.type = sub->type; 
                                    so.startIndex = sub->globalStartIndex; 
                                    so.indexCount = sub->indexCount; 
                                    so.properties = doc->GetElementProperties(sub->guid);
                                    state.objects.push_back(so);
                                } else {
                                    state.objects.erase(std::remove_if(state.objects.begin(), state.objects.end(), [&](const SelectedObject& o) { return o.guid == nodeGuid; }), state.objects.end());
                                }
                                state.selectionChanged = true;
                            }
                            
                            if (ImGui::BeginPopupContextItem(("ctx_" + nodeGuid).c_str())) {
                                if (hasChildren) {
                                    if (ImGui::MenuItem("Select branch")) {
                                        std::vector<std::string> stack = { nodeGuid };
                                        if (!ImGui::GetIO().KeyCtrl) state.objects.clear();
                                        while(!stack.empty()) {
                                            std::string curr = stack.back(); stack.pop_back();
                                            if (geomMap.count(curr) && !state.deletedObjects.count(curr) && !state.hiddenObjects.count(curr)) {
                                                bool isSel = std::any_of(state.objects.begin(), state.objects.end(), [&](const SelectedObject& o) { return o.guid == curr; });
                                                if (!isSel) {
                                                    const auto* s = geomMap[curr];
                                                    SelectedObject so; 
                                                    so.guid = s->guid; 
                                                    so.type = s->type; 
                                                    so.startIndex = s->globalStartIndex; 
                                                    so.indexCount = s->indexCount; 
                                                    so.properties = doc->GetElementProperties(s->guid);
                                                    state.objects.push_back(so);
                                                }
                                            }
                                            auto c = doc->GetChildren(curr);
                                            stack.insert(stack.end(), c.begin(), c.end());
                                        }
                                        state.selectionChanged = true;
                                        triggerFocus = true;
                                    }
                                    if (ImGui::MenuItem("Hide branch")) {
                                        std::vector<std::string> stack = { nodeGuid };
                                        while(!stack.empty()) {
                                            std::string curr = stack.back(); stack.pop_back();
                                            if (geomMap.count(curr)) state.hiddenObjects.insert(curr);
                                            auto c = doc->GetChildren(curr);
                                            stack.insert(stack.end(), c.begin(), c.end());
                                        }
                                        state.hiddenStateChanged = true;
                                    }
                                    if (ImGui::MenuItem("Show branch")) {
                                        std::vector<std::string> stack = { nodeGuid };
                                        while(!stack.empty()) {
                                            std::string curr = stack.back(); stack.pop_back();
                                            if (geomMap.count(curr)) state.hiddenObjects.erase(curr);
                                            auto c = doc->GetChildren(curr);
                                            stack.insert(stack.end(), c.begin(), c.end());
                                        }
                                        state.hiddenStateChanged = true;
                                    }
                                    ImGui::Separator();
                                }
                                
                                if (hasGeom) {
                                    if (isDeleted) {
                                        if (ImGui::MenuItem("Undo delete")) {
                                            state.deletedObjects.erase(nodeGuid);
                                            state.hiddenObjects.erase(nodeGuid);
                                            state.hiddenStateChanged = true;
                                        }
                                    } else {
                                        DrawMultiSelectContextMenu(state, sub, doc, triggerFocus);
                                    }
                                }
                                ImGui::EndPopup();
                            }
                            
                            if (hasChildren && nodeOpen) {
                                for (const auto& child : children) {
                                    self(child, self);
                                }
                            }
                            
                            if (hasChildren && nodeOpen) {
                                ImGui::TreePop();
                            }
                        };

                        for (const auto& root : structuralRoots) {
                            drawSpatialNode(root, drawSpatialNode);
                        }

                        if (!uncategorizedRoots.empty()) {
                            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
                            bool isUncatOpen = ImGui::TreeNodeEx("(Uncategorized Elements)", ImGuiTreeNodeFlags_Framed);
                            ImGui::PopStyleColor();
                            
                            if (isUncatOpen) {
                                for (const auto& root : uncategorizedRoots) {
                                    drawSpatialNode(root, drawSpatialNode);
                                }
                                ImGui::TreePop();
                            }
                        }
                    }
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
            ++it;
        }
        ImGui::EndChild();
    }

} // namespace BimCore