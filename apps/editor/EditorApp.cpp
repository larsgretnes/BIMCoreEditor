// =============================================================================
// BimCore/apps/editor/EditorApp.cpp
// =============================================================================
#include "EditorApp.h"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <iostream>
#include <algorithm>
#include <thread>
#include <fstream>
#include <map>
#include <filesystem>

#include "scene/IfcExporter.h"
#include "scene/CsvImporter.h"
#include "scene/FormatModules.h"
#include "platform/portable-file-dialogs.h"

namespace BimCore {

    struct SelectionBounds {
        glm::vec3 min { kFloatMax, kFloatMax, kFloatMax };
        glm::vec3 max { kFloatMin, kFloatMin, kFloatMin };
        bool      valid = false;
    };

    static SelectionBounds ComputeSelectionBounds(const std::vector<SelectedObject>& objects, const std::vector<Vertex>& masterVerts, const std::vector<uint32_t>& masterIndices) {
        SelectionBounds b;
        for (const auto& obj : objects) {
            for (uint32_t i = 0; i < obj.indexCount; ++i) {
                const uint32_t vi = masterIndices[obj.startIndex + i];
                const float* p  = masterVerts[vi].position;
                for (int j=0; j<3; ++j) {
                    if (p[j] < b.min[j]) b.min[j] = p[j];
                    if (p[j] > b.max[j]) b.max[j] = p[j];
                }
                b.valid = true;
            }
        }
        return b;
    }

    bool EditorApp::Initialize() {
        m_config.Load();

        std::string title = m_config.AppName + " v" + m_config.AppVersion;
        m_window = std::make_unique<Window>(m_config.WindowWidth, m_config.WindowHeight, title.c_str());

        m_graphics = std::make_unique<GraphicsContext>(m_window->GetNativeWindow(), m_config.WindowWidth, m_config.WindowHeight);
        m_graphics->InitImGui(m_window->GetNativeWindow());

        ImGuiStyle& style = ImGui::GetStyle();
        style.Colors[ImGuiCol_WindowBg]         = ImVec4(m_config.ThemeBg[0], m_config.ThemeBg[1], m_config.ThemeBg[2], m_config.ThemeBg[3]);
        style.Colors[ImGuiCol_ChildBg]          = ImVec4(m_config.ThemePanel[0], m_config.ThemePanel[1], m_config.ThemePanel[2], m_config.ThemePanel[3]);

        style.Colors[ImGuiCol_FrameBg]          = ImVec4(m_config.ThemePanel[0]*1.5f, m_config.ThemePanel[1]*1.5f, m_config.ThemePanel[2]*1.5f, 1.0f);
        style.Colors[ImGuiCol_FrameBgHovered]   = ImVec4(m_config.ThemePanel[0]*2.0f, m_config.ThemePanel[1]*2.0f, m_config.ThemePanel[2]*2.0f, 1.0f);

        style.Colors[ImGuiCol_Button]           = ImVec4(m_config.ThemePanel[0]*1.5f, m_config.ThemePanel[1]*1.5f, m_config.ThemePanel[2]*1.5f, 1.0f);
        style.Colors[ImGuiCol_ButtonHovered]    = ImVec4(m_config.ThemeAccent[0]*0.8f, m_config.ThemeAccent[1]*0.8f, m_config.ThemeAccent[2]*0.8f, 1.0f);
        style.Colors[ImGuiCol_ButtonActive]     = ImVec4(m_config.ThemeAccent[0], m_config.ThemeAccent[1], m_config.ThemeAccent[2], 1.0f);

        style.Colors[ImGuiCol_CheckMark]        = ImVec4(m_config.ThemeAccent[0], m_config.ThemeAccent[1], m_config.ThemeAccent[2], 1.0f);

        style.Colors[ImGuiCol_Header]           = ImVec4(m_config.ThemeAccent[0]*0.5f, m_config.ThemeAccent[1]*0.5f, m_config.ThemeAccent[2]*0.5f, 1.0f);
        style.Colors[ImGuiCol_HeaderHovered]    = ImVec4(m_config.ThemeAccent[0]*0.7f, m_config.ThemeAccent[1]*0.7f, m_config.ThemeAccent[2]*0.7f, 1.0f);
        style.Colors[ImGuiCol_HeaderActive]     = ImVec4(m_config.ThemeAccent[0], m_config.ThemeAccent[1], m_config.ThemeAccent[2], 1.0f);
        style.Colors[ImGuiCol_Text]             = ImVec4(m_config.ThemeText[0], m_config.ThemeText[1], m_config.ThemeText[2], m_config.ThemeText[3]);
        style.Colors[ImGuiCol_SliderGrab]       = ImVec4(m_config.ThemeAccent[0], m_config.ThemeAccent[1], m_config.ThemeAccent[2], 1.0f);
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(m_config.ThemeAccent[0]*1.2f, m_config.ThemeAccent[1]*1.2f, m_config.ThemeAccent[2]*1.2f, 1.0f);
        style.FrameRounding = 4.0f;
        style.WindowRounding = 6.0f;
        style.ChildRounding = 4.0f;
        style.GrabRounding = 4.0f;

        m_camera = std::make_unique<Camera>((float)m_config.WindowWidth / (float)m_config.WindowHeight);
        
        m_camera->SetZoomFlyMultiplier(m_config.ZoomFlyMultiplier);
        m_camera->SetFocusSpeed(m_config.CameraFocusSpeed);
        m_camera->SetFocusPadding(m_config.CameraFocusPadding);
        m_camera->SetMinOrbitDistance(m_config.CameraMinOrbitDistance);
        m_camera->SetPivotJumpThreshold(m_config.CameraPivotJumpThreshold);
        m_camera->SetPanReferenceHeight(m_config.CameraPanReferenceHeight);

        m_uiSystem.state.loadState = &m_globalLoadState;

        if (!m_config.AutoLoadPath.empty() && std::filesystem::exists(m_config.AutoLoadPath)) {
            std::lock_guard<std::mutex> lock(m_loadMutex);
            m_safePendingLoadPath = m_config.AutoLoadPath;
        }

        m_lastTime = glfwGetTime();
        return true;
    }

    void EditorApp::RebuildMasterMesh() {
        m_masterVertices.clear();
        m_masterIndices.clear();
        std::vector<TextureData> masterTextures;

        uint32_t vOffset = 0;
        uint32_t iOffset = 0;
        int tOffset = 0;

        float minB[3] = { kFloatMax, kFloatMax, kFloatMax };
        float maxB[3] = { kFloatMin, kFloatMin, kFloatMin };
        bool hasBounds = false;

        for (auto& doc : m_documents) {
            auto& geom = doc->GetGeometry();
            
            m_masterVertices.insert(m_masterVertices.end(), geom.vertices.begin(), geom.vertices.end());
            for (uint32_t idx : geom.indices) m_masterIndices.push_back(idx + vOffset);
            masterTextures.insert(masterTextures.end(), geom.textures.begin(), geom.textures.end());

            for (auto& sub : geom.subMeshes) {
                sub.globalStartIndex = sub.startIndex + iOffset;
                sub.globalTextureIndex = sub.textureIndex >= 0 ? sub.textureIndex + tOffset : -1;
            }

            for (int j=0; j<3; ++j) {
                if (geom.minBounds[j] < minB[j]) minB[j] = geom.minBounds[j];
                if (geom.maxBounds[j] > maxB[j]) maxB[j] = geom.maxBounds[j];
            }
            hasBounds = true;

            vOffset += static_cast<uint32_t>(geom.vertices.size());
            iOffset += static_cast<uint32_t>(geom.indices.size());
            tOffset += static_cast<int>(geom.textures.size());
        }

        m_graphics->UploadMesh(m_masterVertices, m_masterIndices);
        m_graphics->UploadTextures(masterTextures);

        if (hasBounds) {
            m_uiSystem.state.clipXMin = minB[0] - 0.1f; m_uiSystem.state.clipXMax = maxB[0] + 0.1f;
            m_uiSystem.state.clipYMin = minB[1] - 0.1f; m_uiSystem.state.clipYMax = maxB[1] + 0.1f;
            m_uiSystem.state.clipZMin = minB[2] - 0.1f; m_uiSystem.state.clipZMax = maxB[2] + 0.1f;
            for (int j=0; j<3; ++j) {
                m_masterMinBounds[j] = minB[j];
                m_masterMaxBounds[j] = maxB[j];
            }
        }

        m_uiSystem.state.updateGeometry = true; 
        m_triggerRebuild = false;
        m_triggerBatchRebuild = true;
    }

    // --- NEW: Cache GPU batches so we only rebuild indices when visibility changes ---
    void EditorApp::RebuildRenderBatches() {
        m_cachedSolidBatches.clear();
        m_cachedTransBatches.clear();

        for (auto& doc : m_documents) {
            if (doc->IsHidden()) continue;

            for (const auto& sub : doc->GetGeometry().subMeshes) {
                if (m_uiSystem.state.hiddenObjects.count(sub.guid)) continue;
                if (!m_uiSystem.state.showOpeningsAndSpaces && (sub.type == "IfcOpeningElement" || sub.type == "IfcSpace")) continue;

                auto& targetMap = sub.isTransparent ? m_cachedTransBatches : m_cachedSolidBatches;
                auto& targetVec = targetMap[sub.globalTextureIndex];
                
                targetVec.reserve(targetVec.size() + sub.indexCount);
                for (uint32_t i=0; i<sub.indexCount; ++i) {
                    targetVec.push_back(m_masterIndices[sub.globalStartIndex + i]);
                }
            }
        }
        
        m_graphics->UpdateActiveBatches(m_cachedSolidBatches, m_cachedTransBatches);
        m_triggerBatchRebuild = false;
    }

    void EditorApp::Run() {
        while (!m_window->ShouldClose()) {
            double now = glfwGetTime();
            float deltaTime = static_cast<float>(now - m_lastTime);
            m_lastTime = now;

            m_window->PollEvents();
            if (m_window->WasWindowResized()) {
                m_graphics->Resize(m_window->GetWidth(), m_window->GetHeight());
                m_camera->SetAspectRatio((float)m_window->GetWidth() / (float)m_window->GetHeight());
                m_window->ResetWindowResizedFlag();
            }

            HandleAsyncTasks();

            // --- FIXED: Safely intercept visibility toggles to trigger batch rebuilds ---
            if (m_uiSystem.state.hiddenStateChanged) {
                m_triggerBatchRebuild = true;
                m_uiSystem.state.hiddenStateChanged = false;
            }

            if (m_uiSystem.state.triggerResetCamera && !m_documents.empty()) {
                m_uiSystem.state.triggerResetCamera = false;
                
                float minB[3] = { kFloatMax, kFloatMax, kFloatMax };
                float maxB[3] = { kFloatMin, kFloatMin, kFloatMin };
                for (auto& doc : m_documents) {
                    auto& geom = doc->GetGeometry();
                    for (int j=0; j<3; ++j) {
                        if (geom.minBounds[j] < minB[j]) minB[j] = geom.minBounds[j];
                        if (geom.maxBounds[j] > maxB[j]) maxB[j] = geom.maxBounds[j];
                    }
                }
                glm::vec3 globalCenter((minB[0]+maxB[0])*0.5f, (minB[1]+maxB[1])*0.5f, (minB[2]+maxB[2])*0.5f);
                glm::vec3 extents(maxB[0] - minB[0], maxB[1] - minB[1], maxB[2] - minB[2]);
                float radius = glm::length(extents) * 0.5f;

                m_camera->FocusOn(globalCenter, std::max(1.0f, radius));
            }

            m_uiSystem.NewFrame();
            bool triggerFocus = false;

            m_uiSystem.Render(m_uiSystem.state, *m_graphics, m_documents, *m_camera, m_config.MaxExplodeFactor, triggerFocus, m_input.IsFlightMode(), m_triggerRebuild);

            if (m_uiSystem.state.triggerLoad) {
                m_uiSystem.state.triggerLoad = false;
                auto fileDialog = pfd::open_file("Select IFC", m_currentFileDirectory, { "IFC Files", "*.ifc" });
                auto files = fileDialog.result();
                if (!files.empty()) {
                    std::lock_guard<std::mutex> lock(m_loadMutex);
                    m_safePendingLoadPath = files[0];
                }
            }

            if (m_uiSystem.state.triggerImport > 0) {
                int type = m_uiSystem.state.triggerImport;
                m_uiSystem.state.triggerImport = 0;

                std::vector<std::string> filters;
                std::string title;
                if (type == 1) { title = "Import CSV"; filters = { "CSV Files", "*.csv" }; }
                else if (type == 2) { title = "Import BCF"; filters = { "BCF Zip Files", "*.bcf", "BCF XML Files", "*.bcfxml" }; }
                else if (type == 3) { title = "Import glTF"; filters = { "glTF Models", "*.gltf *.glb" }; }

                auto fileDialog = pfd::open_file(title, m_currentFileDirectory, filters);
                auto files = fileDialog.result();

                if (!files.empty()) {
                    if (type == 1) {
                        auto foundGuids = CsvImporter::ExtractGuids(files[0]);
                        if (!foundGuids.empty()) {
                            m_uiSystem.state.objects.clear();
                            for (auto& doc : m_documents) {
                                for (const auto& sub : doc->GetGeometry().subMeshes) {
                                    if (foundGuids.count(sub.guid)) {
                                        SelectedObject so; so.guid = sub.guid; so.type = sub.type; so.startIndex = sub.globalStartIndex; so.indexCount = sub.indexCount; so.properties = doc->GetElementProperties(sub.guid);
                                        m_uiSystem.state.objects.push_back(so);
                                    }
                                }
                            }
                            if (!m_uiSystem.state.objects.empty()) m_uiSystem.state.selectionChanged = true;
                        }
                    } else if (type == 2 && !m_documents.empty()) {
                        BcfImporter::Import(files[0], m_documents[0]);
                    } else if (type == 3) {
                        RenderMesh emptyMesh;
                        auto newGltfDoc = std::make_shared<BimDocument>(nullptr, emptyMesh, files[0]);
                        GltfImporter::Import(files[0], newGltfDoc);
                        m_documents.push_back(newGltfDoc);
                        m_triggerRebuild = true;
                        m_uiSystem.state.triggerResetCamera = true;
                    }
                }
            }

            if (m_uiSystem.state.triggerExport > 0) {
                int type = m_uiSystem.state.triggerExport;
                m_uiSystem.state.triggerExport = 0;

                if (type == 1 && !m_documents.empty()) {
                    std::string defaultName = m_currentFileDirectory + "ModelExport.glb";
                    auto saveDialog = pfd::save_file("Export as glTF", defaultName, { "glTF Binary", "*.glb", "glTF JSON", "*.gltf" });
                    std::string path = saveDialog.result();
                    if (!path.empty()) GltfExporter::Export(path, m_documents[0]);
                }
            }

            HandleSaveTask();
            
            if (m_triggerRebuild) RebuildMasterMesh();
            if (m_triggerBatchRebuild) RebuildRenderBatches();
            
            Update(deltaTime, triggerFocus);
            Render();
        }

        m_graphics->ShutdownImGui();
    }

    void EditorApp::HandleAsyncTasks() {
        std::string triggerPath = "";
        {
            std::lock_guard<std::mutex> lock(m_loadMutex);
            if (!m_safePendingLoadPath.empty()) {
                triggerPath = m_safePendingLoadPath;
                m_safePendingLoadPath = "";
            }
        }

        if (!triggerPath.empty()) {
            m_globalLoadState.Reset();
            size_t pos = triggerPath.find_last_of("/\\");
            m_currentFilename = (pos != std::string::npos) ? triggerPath.substr(pos + 1) : triggerPath;
            m_currentFileDirectory = (pos != std::string::npos) ? triggerPath.substr(0, pos + 1) : "";

            std::thread([this, triggerPath]() {
                auto doc = IfcLoader::LoadDocument(triggerPath, &m_globalLoadState);
                std::lock_guard<std::mutex> lock(m_docMutex);
                m_pendingDoc = doc;
            }).detach();
        }

        {
            std::lock_guard<std::mutex> lock(m_docMutex);
            if (m_pendingDoc) {
                m_documents.push_back(m_pendingDoc);
                m_pendingDoc = nullptr;
                
                m_globalLoadState.isLoaded.store(true);
                m_triggerRebuild = true;
                m_uiSystem.state.triggerResetCamera = true;

                std::string title = m_config.AppName + " v" + m_config.AppVersion + " - " + m_currentFilename;
                glfwSetWindowTitle(m_window->GetNativeWindow(), title.c_str());

                m_uiSystem.state.explodeFactor = 0.0f;
                m_uiSystem.state.selectionChanged = false;

                memset(m_uiSystem.state.globalSearchBuf, 0, sizeof(m_uiSystem.state.globalSearchBuf));
                memset(m_uiSystem.state.localSearchBuf, 0, sizeof(m_uiSystem.state.localSearchBuf));

                m_uiSystem.state.originalProperties.clear();
                m_uiSystem.state.deletedProperties.clear();
                m_uiSystem.state.deletedObjects.clear();
                m_uiSystem.state.hiddenObjects.clear();
                m_uiSystem.state.objects.clear();
                m_uiSystem.state.searchResults.clear();
                m_uiSystem.state.isSearchActive = false;
                m_uiSystem.state.cachedNames.clear();
                m_uiSystem.state.hiddenStateChanged = true;

                m_uiSystem.state.completedMeasurements.clear();
                m_uiSystem.state.isMeasuringActive = false;
            }
        }
    }

    void EditorApp::HandleSaveTask() {
        if (!m_uiSystem.state.triggerSave || m_documents.empty()) return;

        m_uiSystem.state.triggerSave = false;

        std::string defaultSavePath = m_currentFileDirectory + m_currentFilename;
        auto saveDialog = pfd::save_file("Save IFC As", defaultSavePath, { "IFC Files", "*.ifc" });
        std::string savePath = saveDialog.result();

        if (!savePath.empty()) {
            for (const auto& guid : m_uiSystem.state.deletedObjects) {
                for (auto& doc : m_documents) doc->DeleteElement(guid);
                m_uiSystem.state.hiddenObjects.erase(guid);
            }

            for (auto& doc : m_documents) doc->CommitASTChanges();

            std::thread([this, savePath, doc = m_documents[0]]() {
                bool success = IfcExporter::ExportIFC(doc, savePath, &m_globalLoadState);
                if (success) std::cout << "[BIMCore] File saved successfully to " << savePath << "\n";
            }).detach();

            m_uiSystem.state.originalProperties.clear();
            m_uiSystem.state.deletedProperties.clear();
            m_uiSystem.state.deletedObjects.clear();
            m_uiSystem.state.objects.clear();
            m_uiSystem.state.cachedNames.clear();

            size_t pos = savePath.find_last_of("/\\");
            m_currentFilename = (pos != std::string::npos) ? savePath.substr(pos + 1) : savePath;
            m_currentFileDirectory = (pos != std::string::npos) ? savePath.substr(0, pos + 1) : "";

            std::string title = m_config.AppName + " v" + m_config.AppVersion + " - " + m_currentFilename;
            glfwSetWindowTitle(m_window->GetNativeWindow(), title.c_str());        
        }
    }

    void EditorApp::Update(float deltaTime, bool& triggerFocus) {
        auto primaryDoc = m_documents.empty() ? nullptr : m_documents[0];
        m_input.Update(*m_window, *m_camera, m_documents, m_uiSystem.state, m_config, deltaTime, m_currentLightMode, triggerFocus);
        m_camera->Update(deltaTime);

        if (m_documents.empty()) return;

        if (triggerFocus && !m_uiSystem.state.objects.empty()) {
            auto b = ComputeSelectionBounds(m_uiSystem.state.objects, m_masterVertices, m_masterIndices);
            
            if (b.valid) {
                glm::vec3 extents(b.max[0] - b.min[0], b.max[1] - b.min[1], b.max[2] - b.min[2]);
                float radius = glm::length(extents) * 0.5f;
                m_camera->FocusOn((b.min + b.max) * 0.5f, std::max(0.5f, radius));
            }
        }

        UpdateGeometryOffsets();

        m_uiSystem.state.renderMeasurements.clear();
        m_uiSystem.state.drawActiveLine = false;
        m_uiSystem.state.renderSnap.draw = false;

        if (m_uiSystem.state.measureToolActive) {
            glm::mat4 vp = m_camera->GetViewProjectionMatrix();
            float w = (float)m_window->GetWidth();
            float h = (float)m_window->GetHeight();

            auto WorldToScreen = [&](const glm::vec3& pos, float* out) -> bool {
                glm::vec4 clip = vp * glm::vec4(pos, 1.0f);
                if (clip.w <= 0.001f) return false;
                glm::vec3 ndc = glm::vec3(clip) / clip.w;
                out[0] = (ndc.x + 1.0f) * 0.5f * w;
                out[1] = (1.0f - ndc.y) * 0.5f * h;
                return true;
            };

            for (const auto& m : m_uiSystem.state.completedMeasurements) {
                Measurement2D m2d;
                if (WorldToScreen(m.p1, m2d.p1) && WorldToScreen(m.p2, m2d.p2)) {
                    snprintf(m2d.text, sizeof(m2d.text), "%.3f m", glm::length(m.p2 - m.p1));
                    m_uiSystem.state.renderMeasurements.push_back(m2d);
                }
            }

            if (m_uiSystem.state.isMeasuringActive) {
                if (WorldToScreen(m_uiSystem.state.measureStartPoint, m_uiSystem.state.renderActiveLine.p1)) {
                    m_uiSystem.state.drawActiveLine = true;
                    if (m_uiSystem.state.isHoveringGeometry && WorldToScreen(m_uiSystem.state.currentSnapPoint, m_uiSystem.state.renderActiveLine.p2)) {
                        snprintf(m_uiSystem.state.renderActiveLine.text, sizeof(m_uiSystem.state.renderActiveLine.text), "%.3f m", glm::length(m_uiSystem.state.currentSnapPoint - m_uiSystem.state.measureStartPoint));
                    } else {
                        m_uiSystem.state.renderActiveLine.text[0] = '\0';
                    }
                }
            }

            if (m_uiSystem.state.isHoveringGeometry) {
                m_uiSystem.state.renderSnap.type = m_uiSystem.state.currentSnapType;
                if (WorldToScreen(m_uiSystem.state.currentSnapPoint, m_uiSystem.state.renderSnap.p)) {
                    m_uiSystem.state.renderSnap.draw = true;
                    if (m_uiSystem.state.currentSnapType == SnapType::Edge) {
                        WorldToScreen(m_uiSystem.state.currentSnapEdgeV0, m_uiSystem.state.renderSnap.e0);
                        WorldToScreen(m_uiSystem.state.currentSnapEdgeV1, m_uiSystem.state.renderSnap.e1);
                    }
                }
            }
        }
    }

    void EditorApp::UpdateGeometryOffsets() {
        if (!m_uiSystem.state.updateGeometry) return;

        float oldMin[3] = { kFloatMax, kFloatMax, kFloatMax };
        float oldMax[3] = { kFloatMin, kFloatMin, kFloatMin };
        
        if (!m_masterVertices.empty()) {
            for (const auto& v : m_masterVertices) {
                for (int j=0; j<3; ++j) {
                    if (v.position[j] < oldMin[j]) oldMin[j] = v.position[j];
                    if (v.position[j] > oldMax[j]) oldMax[j] = v.position[j];
                }
            }
        } else {
            for (auto& doc : m_documents) {
                auto& geom = doc->GetGeometry();
                for (int j=0; j<3; ++j) {
                    if (geom.minBounds[j] < oldMin[j]) oldMin[j] = geom.minBounds[j];
                    if (geom.maxBounds[j] > oldMax[j]) oldMax[j] = geom.maxBounds[j];
                }
            }
        }

        for (int j=0; j<3; ++j) {
            oldMin[j] -= 0.1f; 
            oldMax[j] += 0.1f;
            if (oldMax[j] - oldMin[j] < 0.0001f) {
                oldMin[j] -= 0.1f; oldMax[j] += 0.1f;
            }
        }

        float pctXMin = std::clamp((m_uiSystem.state.clipXMin - oldMin[0]) / (oldMax[0] - oldMin[0]), 0.0f, 1.0f);
        float pctXMax = std::clamp((m_uiSystem.state.clipXMax - oldMin[0]) / (oldMax[0] - oldMin[0]), 0.0f, 1.0f);
        float pctYMin = std::clamp((m_uiSystem.state.clipYMin - oldMin[1]) / (oldMax[1] - oldMin[1]), 0.0f, 1.0f);
        float pctYMax = std::clamp((m_uiSystem.state.clipYMax - oldMin[1]) / (oldMax[1] - oldMin[1]), 0.0f, 1.0f);
        float pctZMin = std::clamp((m_uiSystem.state.clipZMin - oldMin[2]) / (oldMax[2] - oldMin[2]), 0.0f, 1.0f);
        float pctZMax = std::clamp((m_uiSystem.state.clipZMax - oldMin[2]) / (oldMax[2] - oldMin[2]), 0.0f, 1.0f);

        float origMin[3] = { kFloatMax, kFloatMax, kFloatMax };
        float origMax[3] = { kFloatMin, kFloatMin, kFloatMin };
        for (auto& doc : m_documents) {
            auto& geom = doc->GetGeometry();
            for (int j=0; j<3; ++j) {
                if (geom.minBounds[j] < origMin[j]) origMin[j] = geom.minBounds[j];
                if (geom.maxBounds[j] > origMax[j]) origMax[j] = geom.maxBounds[j];
            }
        }
        glm::vec3 globalCenter((origMin[0]+origMax[0])*0.5f, (origMin[1]+origMax[1])*0.5f, (origMin[2]+origMax[2])*0.5f);

        m_masterVertices.clear();
        float newMin[3] = { kFloatMax, kFloatMax, kFloatMax };
        float newMax[3] = { kFloatMin, kFloatMin, kFloatMin };

        for (auto& doc : m_documents) {
            auto& geom = doc->GetGeometry();
            geom.vertices = geom.originalVertices;

            if (m_uiSystem.state.explodeFactor > 0.01f) {
                std::vector<bool> shifted(geom.vertices.size(), false);
                for (const auto& sub : geom.subMeshes) {
                    glm::vec3 subCenter(sub.center[0], sub.center[1], sub.center[2]);
                    glm::vec3 dir = subCenter - globalCenter;
                    glm::vec3 offset = dir * m_uiSystem.state.explodeFactor;

                    for (uint32_t i = 0; i < sub.indexCount; ++i) {
                        uint32_t vIdx = geom.indices[sub.startIndex + i];
                        if (!shifted[vIdx]) {
                            geom.vertices[vIdx].position[0] += offset.x;
                            geom.vertices[vIdx].position[1] += offset.y;
                            geom.vertices[vIdx].position[2] += offset.z;
                            shifted[vIdx] = true;
                        }
                    }
                }
            }
            
            for (const auto& v : geom.vertices) {
                for(int j=0; j<3; ++j) {
                    if (v.position[j] < newMin[j]) newMin[j] = v.position[j];
                    if (v.position[j] > newMax[j]) newMax[j] = v.position[j];
                }
            }
            m_masterVertices.insert(m_masterVertices.end(), geom.vertices.begin(), geom.vertices.end());
        }

        // --- FIXED: Safely cache the physical bounds of the Live Master Mesh ---
        for (int j=0; j<3; ++j) {
            newMin[j] -= 0.1f; 
            newMax[j] += 0.1f;
            if (newMax[j] - newMin[j] < 0.0001f) {
                newMin[j] -= 0.1f; newMax[j] += 0.1f;
            }
            m_masterMinBounds[j] = newMin[j];
            m_masterMaxBounds[j] = newMax[j];
        }

        m_uiSystem.state.clipXMin = newMin[0] + pctXMin * (newMax[0] - newMin[0]);
        m_uiSystem.state.clipXMax = newMin[0] + pctXMax * (newMax[0] - newMin[0]);
        m_uiSystem.state.clipYMin = newMin[1] + pctYMin * (newMax[1] - newMin[1]);
        m_uiSystem.state.clipYMax = newMin[1] + pctYMax * (newMax[1] - newMin[1]);
        m_uiSystem.state.clipZMin = newMin[2] + pctZMin * (newMax[2] - newMin[2]);
        m_uiSystem.state.clipZMax = newMin[2] + pctZMax * (newMax[2] - newMin[2]);

        m_graphics->UpdateGeometry(m_masterVertices);
        m_uiSystem.state.updateGeometry = false;
    }

    void EditorApp::Render() {
        if (m_documents.empty()) {
            SceneUniforms defaultScene{};
            defaultScene.viewProjection = m_camera->GetViewProjectionMatrix();
            defaultScene.invViewProjection = glm::inverse(defaultScene.viewProjection);
            defaultScene.screenWidth = m_window->GetWidth();
            defaultScene.screenHeight = m_window->GetHeight();
            m_graphics->UpdateScene(defaultScene);
            m_graphics->RenderFrame();
            return;
        }

        if (m_uiSystem.state.showBoundingBox && !m_uiSystem.state.objects.empty()) {
            auto b = ComputeSelectionBounds(m_uiSystem.state.objects, m_masterVertices, m_masterIndices);
            if (b.valid) m_graphics->SetBoundingBox(true, b.min, b.max);
        } else {
            m_graphics->SetBoundingBox(false, glm::vec3(0), glm::vec3(0));
        }

        if (!m_uiSystem.state.objects.empty()) {
            std::vector<HighlightRange> ranges;
            for(auto& o : m_uiSystem.state.objects) ranges.push_back({ o.startIndex, o.indexCount });
            m_graphics->SetHighlight(true, ranges, m_uiSystem.state.style);
        } else {
            m_graphics->SetHighlight(false, {}, 0);
        }

        // --- FIXED: Read directly from the cached Master Bounds arrays! ---
        m_graphics->SetClippingPlanes(
            m_uiSystem.state.showPlaneXMin, m_uiSystem.state.clipXMin, m_uiSystem.state.showPlaneXMax, m_uiSystem.state.clipXMax, m_uiSystem.state.planeColorX,
            m_uiSystem.state.showPlaneYMin, m_uiSystem.state.clipYMin, m_uiSystem.state.showPlaneYMax, m_uiSystem.state.clipYMax, m_uiSystem.state.planeColorY,
            m_uiSystem.state.showPlaneZMin, m_uiSystem.state.clipZMin, m_uiSystem.state.showPlaneZMax, m_uiSystem.state.clipZMax, m_uiSystem.state.planeColorZ,
            glm::vec3(m_masterMinBounds[0], m_masterMinBounds[1], m_masterMinBounds[2]), 
            glm::vec3(m_masterMaxBounds[0], m_masterMaxBounds[1], m_masterMaxBounds[2])
        );

        SceneUniforms scene{};
        scene.viewProjection = m_camera->GetViewProjectionMatrix();

        scene.invViewProjection = glm::inverse(scene.viewProjection);
        scene.screenWidth = m_window->GetWidth();
        scene.screenHeight = m_window->GetHeight();

        scene.lightingMode = m_currentLightMode;
        scene.highlightColor = m_uiSystem.state.color;

        scene.sunDirection = glm::vec4(normalize(glm::vec3(0.5f, 0.8f, 0.3f)), 0.0f);

        scene.clipActiveMin = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        scene.clipActiveMax = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

        scene.clipMin.x = m_uiSystem.state.clipXMin; scene.clipMax.x = m_uiSystem.state.clipXMax;
        scene.clipMin.y = m_uiSystem.state.clipYMin; scene.clipMax.y = m_uiSystem.state.clipYMax;
        scene.clipMin.z = m_uiSystem.state.clipZMin; scene.clipMax.z = m_uiSystem.state.clipZMax;

        m_graphics->UpdateScene(scene);
        m_graphics->RenderFrame();
    }

} // namespace BimCore