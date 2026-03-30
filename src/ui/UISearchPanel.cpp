// =============================================================================
// BimCore/apps/editor/ui/UISearchPanel.cpp
// =============================================================================
#include "UISearchPanel.h"
#include <imgui.h>
#include <string>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <vector>
#include <set>
#include "platform/portable-file-dialogs.h"

namespace BimCore {

    // --- Helper function for Tooltip ---
    static void HelpMarker(const char* desc) {
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    void UISearchPanel::ParseSearchQuery(const std::string& query, std::string& outKey, std::string& outValue) {
        outKey.clear();
        outValue.clear();

        if (query.empty()) return;

        bool inQuotes = false;
        int splitIndex = -1;

        for (int i = 0; i < query.length(); ++i) {
            if (query[i] == '"') {
                inQuotes = !inQuotes;
            } else if (query[i] == ':' && !inQuotes) {
                splitIndex = i;
                break;
            }
        }

        if (splitIndex != -1) {
            outKey = query.substr(0, splitIndex);
            outValue = query.substr(splitIndex + 1);
        } else {
            outValue = query; 
        }

        auto trimAndClean = [](std::string s) {
            s.erase(0, s.find_first_not_of(" \t"));
            s.erase(s.find_last_not_of(" \t") + 1);
            if (s.length() >= 2 && s.front() == '"' && s.back() == '"') s = s.substr(1, s.length() - 2);
            s.erase(std::remove(s.begin(), s.end(), '*'), s.end());
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            return s;
        };

        outKey = trimAndClean(outKey);
        outValue = trimAndClean(outValue);
    }

    void UISearchPanel::ExecuteTextSearch(const std::string& query, std::vector<std::shared_ptr<SceneModel>>& documents, SelectionState& state) {
        state.objects.clear();
        
        std::string searchKey, searchValue;
        ParseSearchQuery(query, searchKey, searchValue);

        if (searchValue.empty() && searchKey.empty()) return;

        for (auto& doc : documents) {
            if (doc->IsHidden()) continue;
            const auto& geom = doc->GetGeometry();
            for (const auto& sub : geom.subMeshes) {
                if (state.hiddenObjects.count(sub.guid)) continue;

                std::string bg = sub.guid.length() >= 22 ? sub.guid.substr(0, 22) : sub.guid;
                std::string bgLower = bg;
                std::transform(bgLower.begin(), bgLower.end(), bgLower.begin(), ::tolower);
                
                std::string typeLower = sub.type;
                std::transform(typeLower.begin(), typeLower.end(), typeLower.begin(), ::tolower);
                
                auto props = doc->GetElementProperties(bg);
                bool match = false;

                if (searchKey.empty()) {
                    if (bgLower.find(searchValue) != std::string::npos || typeLower.find(searchValue) != std::string::npos) {
                        match = true;
                    } else {
                        for (const auto& [k, prop] : props) {
                            std::string valLower = prop.value; 
                            std::transform(valLower.begin(), valLower.end(), valLower.begin(), ::tolower);
                            if (valLower.find(searchValue) != std::string::npos) {
                                match = true;
                                break;
                            }
                        }
                    }
                } else {
                    if (std::string("guid").find(searchKey) != std::string::npos && bgLower.find(searchValue) != std::string::npos) match = true;
                    if (!match && std::string("type").find(searchKey) != std::string::npos && typeLower.find(searchValue) != std::string::npos) match = true;
                    
                    if (!match) {
                        for (const auto& [k, prop] : props) {
                            std::string kLower = k; 
                            std::transform(kLower.begin(), kLower.end(), kLower.begin(), ::tolower);
                            
                            if (kLower.find(searchKey) != std::string::npos) {
                                std::string valLower = prop.value;
                                std::transform(valLower.begin(), valLower.end(), valLower.begin(), ::tolower);
                                
                                if (valLower.find(searchValue) != std::string::npos) {
                                    match = true;
                                    break;
                                }
                            }
                        }
                    }
                }

                if (match) {
                    SelectedObject so;
                    so.guid = sub.guid;
                    so.type = sub.type;
                    so.startIndex = sub.globalStartIndex;
                    so.indexCount = sub.indexCount;
                    so.properties = props;
                    state.objects.push_back(so);
                }
            }
        }
        
        state.selectionChanged = true;
    }

    void UISearchPanel::Render(SelectionState& state, std::vector<std::shared_ptr<SceneModel>>& documents, bool& triggerFocus) {
        
        ImGui::Begin("Advanced Search", &state.showSearchPanel);

        // =================================================================
        // 1. Text Search (Default Open)
        // =================================================================
        ImGui::TextDisabled("Text Search"); 
        ImGui::SameLine();
        HelpMarker("Search Tips:\n"
                   "- 'chair' finds anything containing the word.\n"
                   "- 'Name:Chair' searches specifically in the Name property.\n"
                   "- '\"Pset_Room:Name\":Office' supports special characters.\n"
                   "- Wildcards like '*' are ignored; implicit 'contains' is used.");

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 60.0f);
        bool doTextSearch = ImGui::InputTextWithHint("##globalsearch", "Search Key:Value or Global...", state.globalSearchBuf, sizeof(state.globalSearchBuf), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("Find", ImVec2(55, 0))) doTextSearch = true;

        if (doTextSearch) {
            ExecuteTextSearch(state.globalSearchBuf, documents, state);
            if (!state.objects.empty()) triggerFocus = true;
        }

        if (!state.objects.empty()) {
            ImGui::Spacing();
            if (ImGui::Button("Export Results to CSV", ImVec2(-FLT_MIN, 0))) {
                auto saveDialog = pfd::save_file("Export Search Results", "SearchResults.csv", { "CSV Files", "*.csv" });
                std::string path = saveDialog.result();
                
                if (!path.empty()) {
                    std::ofstream out(path);

                    auto escapeCSV = [](const std::string& val) {
                        std::string res = val;
                        size_t pos = 0;
                        while ((pos = res.find('"', pos)) != std::string::npos) {
                            res.replace(pos, 1, "\"\"");
                            pos += 2;
                        }
                        return "\"" + res + "\"";
                    };

                    std::vector<std::string> customProps;
                    std::set<std::string> seenProps;
                    for (const auto& obj : state.objects) {
                        for (const auto& [key, prop] : obj.properties) {
                            if (key != "Name" && seenProps.find(key) == seenProps.end()) {
                                seenProps.insert(key);
                                customProps.push_back(key);
                            }
                        }
                    }
                    
                    std::sort(customProps.begin(), customProps.end());

                    out << "GUID,Type,Name";
                    for (const auto& key : customProps) {
                        out << "," << escapeCSV(key);
                    }
                    out << "\n";

                    for (const auto& obj : state.objects) {
                        std::string name = obj.properties.count("Name") ? obj.properties.at("Name").value : "";
                        out << escapeCSV(obj.guid) << "," << escapeCSV(obj.type) << "," << escapeCSV(name);
                        
                        for (const auto& key : customProps) {
                            if (obj.properties.count(key)) {
                                out << "," << escapeCSV(obj.properties.at(key).value);
                            } else {
                                out << ","; 
                            }
                        }
                        out << "\n";
                    }
                    out.close();
                }
            }
        }

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        // =================================================================
        // 2. Object-Based Spatial Search (Collapsible)
        // =================================================================
        if (ImGui::CollapsingHeader("Spatial Search")) {
            if (state.objects.empty()) {
                ImGui::TextWrapped("Select one or more elements first to use their combined bounding box for a spatial search.");
            } else {
                static int spatialMode = 1; 
                static bool invertSpatial = false;
                
                ImGui::RadioButton("Inside Box", &spatialMode, 0); ImGui::SameLine();
                ImGui::RadioButton("Touching Box", &spatialMode, 1);
                ImGui::Checkbox("Invert Selection Result", &invertSpatial);

                if (ImGui::Button("Search by Target Volume", ImVec2(-FLT_MIN, 0))) {
                    
                    float tMin[3] = { 1e9f, 1e9f, 1e9f };
                    float tMax[3] = {-1e9f,-1e9f,-1e9f };
                    bool hasTarget = false;

                    for (auto& doc : documents) {
                        const auto& geom = doc->GetGeometry();
                        for (const auto& obj : state.objects) {
                            for (const auto& sub : geom.subMeshes) {
                                if (sub.guid == obj.guid) {
                                    for(uint32_t i = 0; i < sub.indexCount; ++i) {
                                        const float* p = geom.vertices[geom.indices[sub.startIndex + i]].position;
                                        for(int j=0; j<3; ++j) {
                                            if(p[j] < tMin[j]) tMin[j] = p[j];
                                            if(p[j] > tMax[j]) tMax[j] = p[j];
                                        }
                                    }
                                    hasTarget = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (hasTarget) {
                        std::vector<SelectedObject> newSelection;
                        
                        for (auto& doc : documents) {
                            if (doc->IsHidden()) continue;
                            const auto& geom = doc->GetGeometry();

                            for (const auto& sub : geom.subMeshes) {
                                if (state.hiddenObjects.count(sub.guid)) continue;
                                if (!state.showOpeningsAndSpaces && (sub.type == "IfcOpeningElement" || sub.type == "IfcSpace")) continue;

                                float sMin[3] = { 1e9f, 1e9f, 1e9f };
                                float sMax[3] = {-1e9f,-1e9f,-1e9f };

                                if (!geom.bvhNodes.empty() && sub.bvhRootIndex < geom.bvhNodes.size()) {
                                    const auto& node = geom.bvhNodes[sub.bvhRootIndex];
                                    for(int j=0; j<3; ++j) { sMin[j] = node.aabbMin[j]; sMax[j] = node.aabbMax[j]; }
                                } else {
                                    for(uint32_t i = 0; i < sub.indexCount; ++i) {
                                        const float* p = geom.vertices[geom.indices[sub.startIndex + i]].position;
                                        for(int j=0; j<3; ++j) {
                                            if(p[j] < sMin[j]) sMin[j] = p[j];
                                            if(p[j] > sMax[j]) sMax[j] = p[j];
                                        }
                                    }
                                }

                                bool match = false;
                                if (spatialMode == 0) { 
                                    match = (sMin[0] >= tMin[0] && sMax[0] <= tMax[0] &&
                                             sMin[1] >= tMin[1] && sMax[1] <= tMax[1] &&
                                             sMin[2] >= tMin[2] && sMax[2] <= tMax[2]);
                                } else { 
                                    match = (sMin[0] <= tMax[0] && sMax[0] >= tMin[0] &&
                                             sMin[1] <= tMax[1] && sMax[1] >= tMin[1] &&
                                             sMin[2] <= tMax[2] && sMax[2] >= tMin[2]);
                                }

                                if (invertSpatial) match = !match;

                                if (match) {
                                    SelectedObject so;
                                    so.guid = sub.guid;
                                    so.type = sub.type;
                                    so.startIndex = sub.globalStartIndex;
                                    so.indexCount = sub.indexCount;
                                    std::string bg = sub.guid.length() >= 22 ? sub.guid.substr(0, 22) : sub.guid;
                                    so.properties = doc->GetElementProperties(bg);
                                    newSelection.push_back(so);
                                }
                            }
                        }
                        
                        state.objects = newSelection;
                        state.selectionChanged = true;
                        if (!state.objects.empty()) triggerFocus = true;
                    }
                }
            }
        }

        // =================================================================
        // 3. Clipping Plane Search (Collapsible)
        // =================================================================
        if (ImGui::CollapsingHeader("Clipping Planes Box Search")) {
            bool anyClipActive = state.showPlaneXMin || state.showPlaneXMax ||
                                 state.showPlaneYMin || state.showPlaneYMax ||
                                 state.showPlaneZMin || state.showPlaneZMax;

            if (!anyClipActive) {
                ImGui::TextWrapped("Activate at least one clipping plane to use this feature.");
            } else {
                static bool fullyInsidePlanes = false;
                ImGui::Checkbox("Must be completely inside slice", &fullyInsidePlanes);

                if (ImGui::Button("Search by Sliced Volume", ImVec2(-FLT_MIN, 0))) {
                    std::vector<SelectedObject> newSelection;

                    for (auto& doc : documents) {
                        if (doc->IsHidden()) continue;
                        const auto& geom = doc->GetGeometry();

                        for (const auto& sub : geom.subMeshes) {
                            if (state.hiddenObjects.count(sub.guid)) continue;
                            if (!state.showOpeningsAndSpaces && (sub.type == "IfcOpeningElement" || sub.type == "IfcSpace")) continue;

                            float sMin[3] = { 1e9f, 1e9f, 1e9f };
                            float sMax[3] = {-1e9f,-1e9f,-1e9f };

                            if (!geom.bvhNodes.empty() && sub.bvhRootIndex < geom.bvhNodes.size()) {
                                const auto& node = geom.bvhNodes[sub.bvhRootIndex];
                                for(int j=0; j<3; ++j) { sMin[j] = node.aabbMin[j]; sMax[j] = node.aabbMax[j]; }
                            } else {
                                for(uint32_t i = 0; i < sub.indexCount; ++i) {
                                    const float* p = geom.vertices[geom.indices[sub.startIndex + i]].position;
                                    for(int j=0; j<3; ++j) {
                                        if(p[j] < sMin[j]) sMin[j] = p[j];
                                        if(p[j] > sMax[j]) sMax[j] = p[j];
                                    }
                                }
                            }

                            bool match = true;
                            
                            if (fullyInsidePlanes) {
                                if (state.showPlaneXMin && sMin[0] < state.clipXMin) match = false;
                                if (state.showPlaneXMax && sMax[0] > state.clipXMax) match = false;
                                if (state.showPlaneYMin && sMin[1] < state.clipYMin) match = false;
                                if (state.showPlaneYMax && sMax[1] > state.clipYMax) match = false;
                                if (state.showPlaneZMin && sMin[2] < state.clipZMin) match = false;
                                if (state.showPlaneZMax && sMax[2] > state.clipZMax) match = false;
                            } else {
                                if (state.showPlaneXMin && sMax[0] < state.clipXMin) match = false;
                                if (state.showPlaneXMax && sMin[0] > state.clipXMax) match = false;
                                if (state.showPlaneYMin && sMax[1] < state.clipYMin) match = false;
                                if (state.showPlaneYMax && sMin[1] > state.clipYMax) match = false;
                                if (state.showPlaneZMin && sMax[2] < state.clipZMin) match = false;
                                if (state.showPlaneZMax && sMin[2] > state.clipZMax) match = false;
                            }

                            if (match) {
                                SelectedObject so;
                                so.guid = sub.guid;
                                so.type = sub.type;
                                so.startIndex = sub.globalStartIndex;
                                so.indexCount = sub.indexCount;
                                std::string bg = sub.guid.length() >= 22 ? sub.guid.substr(0, 22) : sub.guid;
                                so.properties = doc->GetElementProperties(bg);
                                newSelection.push_back(so);
                            }
                        }
                    }

                    state.objects = newSelection;
                    state.selectionChanged = true;
                    if (!state.objects.empty()) triggerFocus = true;
                }
            }
        }

        ImGui::End();
    }

} // namespace BimCore