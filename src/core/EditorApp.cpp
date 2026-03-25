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
#include <filesystem>

#include "io/IfcExporter.h"
#include "io/CsvImporter.h"
#include "io/BcfImporter.h"
#include "io/GltfImporter.h"
#include "io/GltfExporter.h"
#include "platform/portable-file-dialogs.h"

namespace BimCore {

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

            if (m_uiSystem.state.hiddenStateChanged) {
                m_sceneContext.triggerBatchRebuild = true;
                m_uiSystem.state.hiddenStateChanged = false;
            }

            m_uiSystem.NewFrame();
            bool triggerFocus = false;

            m_uiSystem.Render(m_uiSystem.state, *m_graphics, m_sceneContext.GetDocuments(), *m_camera, m_config.MaxExplodeFactor, triggerFocus, m_input.IsFlightMode(), m_sceneContext.triggerRebuild, &m_commandHistory);

            if (m_uiSystem.state.triggerLoad) {
                m_uiSystem.state.triggerLoad = false;
                auto fileDialog = pfd::open_file("Select File", m_currentFileDirectory, { "Supported Files", "*.ifc *.gltf *.glb" });
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

                auto fileDialog = pfd::open_file(title, m_currentFileDirectory, filters);
                auto files = fileDialog.result();

                if (!files.empty()) {
                    if (type == 1) {
                        auto foundGuids = CsvImporter::ExtractGuids(files[0]);
                        if (!foundGuids.empty()) {
                            m_uiSystem.state.objects.clear();
                            for (auto& doc : m_sceneContext.GetDocuments()) {
                                for (const auto& sub : doc->GetGeometry().subMeshes) {
                                    if (foundGuids.count(sub.guid)) {
                                        SelectedObject so; so.guid = sub.guid; so.type = sub.type; so.startIndex = sub.globalStartIndex; so.indexCount = sub.indexCount; so.properties = doc->GetElementProperties(sub.guid);
                                        m_uiSystem.state.objects.push_back(so);
                                    }
                                }
                            }
                            if (!m_uiSystem.state.objects.empty()) m_uiSystem.state.selectionChanged = true;
                        }
                    } else if (type == 2 && !m_sceneContext.GetDocuments().empty()) {
                        BcfImporter::Import(files[0], m_sceneContext.GetDocuments()[0]);
                    }
                }
            }

            if (m_uiSystem.state.triggerExport > 0) {
                int type = m_uiSystem.state.triggerExport;
                m_uiSystem.state.triggerExport = 0;

                if (type == 1 && !m_sceneContext.GetDocuments().empty()) {
                    std::string defaultName = m_currentFileDirectory + "ModelExport.glb";
                    auto saveDialog = pfd::save_file("Export as glTF", defaultName, { "glTF Binary", "*.glb", "glTF JSON", "*.gltf" });
                    std::string path = saveDialog.result();
                    if (!path.empty()) GltfExporter::Export(path, m_sceneContext.GetDocuments()[0]);
                }
            }

            HandleSaveTask();
            
            if (m_sceneContext.triggerRebuild) m_sceneContext.RebuildMasterMesh(m_graphics.get(), m_uiSystem.state);
            if (m_sceneContext.triggerBatchRebuild) m_sceneContext.RebuildRenderBatches(m_graphics.get(), m_uiSystem.state);

            if (m_uiSystem.state.triggerResetCamera && !m_sceneContext.GetDocuments().empty()) {
                m_uiSystem.state.triggerResetCamera = false;
                
                glm::vec3 globalCenter((m_sceneContext.minBounds[0]+m_sceneContext.maxBounds[0])*0.5f, 
                                       (m_sceneContext.minBounds[1]+m_sceneContext.maxBounds[1])*0.5f, 
                                       (m_sceneContext.minBounds[2]+m_sceneContext.maxBounds[2])*0.5f);
                glm::vec3 extents(m_sceneContext.maxBounds[0] - m_sceneContext.minBounds[0], 
                                  m_sceneContext.maxBounds[1] - m_sceneContext.minBounds[1], 
                                  m_sceneContext.maxBounds[2] - m_sceneContext.minBounds[2]);
                float radius = glm::length(extents) * 0.5f;

                m_camera->FocusOn(globalCenter, std::max(1.0f, radius));
            }
            
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

            std::string ext = triggerPath.substr(triggerPath.find_last_of(".") + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            if (ext == "gltf" || ext == "glb") {
                RenderMesh emptyMesh;
                auto newGltfDoc = std::make_shared<SceneModel>(nullptr, emptyMesh, triggerPath);
                GltfImporter::Import(triggerPath, newGltfDoc);
                std::lock_guard<std::mutex> lock(m_docMutex);
                m_pendingDoc = newGltfDoc;
            } else {
                std::thread([this, triggerPath]() {
                    auto doc = IfcLoader::LoadDocument(triggerPath, &m_globalLoadState);
                    std::lock_guard<std::mutex> lock(m_docMutex);
                    m_pendingDoc = doc;
                }).detach();
            }
        }

        {
            std::lock_guard<std::mutex> lock(m_docMutex);
            if (m_pendingDoc) {
                m_sceneContext.AddDocument(m_pendingDoc);
                m_pendingDoc = nullptr;
                
                m_globalLoadState.isLoaded.store(true);
                m_sceneContext.triggerRebuild = true;
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
                
                m_commandHistory.Clear();
            }
        }
    }

    void EditorApp::HandleSaveTask() {
        if (!m_uiSystem.state.triggerSave || m_sceneContext.GetDocuments().empty()) return;

        m_uiSystem.state.triggerSave = false;

        std::string defaultSavePath = m_currentFileDirectory + m_currentFilename;
        auto saveDialog = pfd::save_file("Save IFC As", defaultSavePath, { "IFC Files", "*.ifc" });
        std::string savePath = saveDialog.result();

        if (!savePath.empty()) {
            for (const auto& guid : m_uiSystem.state.deletedObjects) {
                for (auto& doc : m_sceneContext.GetDocuments()) doc->DeleteElement(guid);
                m_uiSystem.state.hiddenObjects.erase(guid);
            }

            for (auto& doc : m_sceneContext.GetDocuments()) doc->CommitASTChanges();

            std::thread([this, savePath, doc = m_sceneContext.GetDocuments()[0]]() {
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
        auto primaryDoc = m_sceneContext.GetDocuments().empty() ? nullptr : m_sceneContext.GetDocuments()[0];
        
        m_input.Update(*m_window, *m_camera, m_sceneContext.GetDocuments(), m_uiSystem.state, m_config, deltaTime, m_currentLightMode, triggerFocus, m_commandHistory);
        m_camera->Update(deltaTime);

        if (m_sceneContext.GetDocuments().empty()) return;

        if (triggerFocus && !m_uiSystem.state.objects.empty()) {
            auto b = m_sceneContext.ComputeSelectionBounds(m_uiSystem.state.objects);
            if (b.valid) {
                glm::vec3 extents(b.max[0] - b.min[0], b.max[1] - b.min[1], b.max[2] - b.min[2]);
                float radius = glm::length(extents) * 0.5f;
                m_camera->FocusOn((b.min + b.max) * 0.5f, std::max(0.5f, radius));
            }
        }

        m_sceneContext.UpdateGeometryOffsets(m_graphics.get(), m_uiSystem.state, m_uiSystem.state.explodeFactor);

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

    void EditorApp::Render() {
        if (m_sceneContext.GetDocuments().empty()) {
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
            auto b = m_sceneContext.ComputeSelectionBounds(m_uiSystem.state.objects);
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

        bool showClips = (m_uiSystem.state.activeTool == InteractionTool::Select);

        m_graphics->SetClippingPlanes(
            showClips && m_uiSystem.state.showPlaneXMin, m_uiSystem.state.clipXMin, showClips && m_uiSystem.state.showPlaneXMax, m_uiSystem.state.clipXMax, m_uiSystem.state.planeColorX,
            showClips && m_uiSystem.state.showPlaneYMin, m_uiSystem.state.clipYMin, showClips && m_uiSystem.state.showPlaneYMax, m_uiSystem.state.clipYMax, m_uiSystem.state.planeColorY,
            showClips && m_uiSystem.state.showPlaneZMin, m_uiSystem.state.clipZMin, showClips && m_uiSystem.state.showPlaneZMax, m_uiSystem.state.clipZMax, m_uiSystem.state.planeColorZ,
            glm::vec3(m_sceneContext.minBounds[0], m_sceneContext.minBounds[1], m_sceneContext.minBounds[2]), 
            glm::vec3(m_sceneContext.maxBounds[0], m_sceneContext.maxBounds[1], m_sceneContext.maxBounds[2])
        );

        SceneUniforms scene{};
        scene.viewProjection = m_camera->GetViewProjectionMatrix();
        scene.invViewProjection = glm::inverse(scene.viewProjection);
        scene.screenWidth = m_window->GetWidth();
        scene.screenHeight = m_window->GetHeight();
        scene.lightingMode = m_currentLightMode;
        scene.highlightColor = m_uiSystem.state.color;
        scene.sunDirection = glm::vec4(glm::normalize(glm::vec3(0.5f, 0.8f, 0.3f)), 0.0f);

        glm::vec3 globalCenter((m_sceneContext.minBounds[0] + m_sceneContext.maxBounds[0]) * 0.5f, 
                               (m_sceneContext.minBounds[1] + m_sceneContext.maxBounds[1]) * 0.5f, 
                               (m_sceneContext.minBounds[2] + m_sceneContext.maxBounds[2]) * 0.5f);
        float radius = glm::length(glm::vec3(m_sceneContext.maxBounds[0] - m_sceneContext.minBounds[0], 
                                             m_sceneContext.maxBounds[1] - m_sceneContext.minBounds[1], 
                                             m_sceneContext.maxBounds[2] - m_sceneContext.minBounds[2])) * 0.5f;
        glm::vec3 lightDir = glm::vec3(scene.sunDirection);
        glm::mat4 lightView = glm::lookAt(globalCenter + lightDir * radius, globalCenter, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 lightProj = glm::ortho(-radius, radius, -radius, radius, 0.1f, radius * 2.0f);
        scene.lightSpaceMatrix = lightProj * lightView;

        scene.clipActiveMin = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        scene.clipActiveMax = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

        scene.clipMin.x = m_uiSystem.state.clipXMin; scene.clipMax.x = m_uiSystem.state.clipXMax;
        scene.clipMin.y = m_uiSystem.state.clipYMin; scene.clipMax.y = m_uiSystem.state.clipYMax;
        scene.clipMin.z = m_uiSystem.state.clipZMin; scene.clipMax.z = m_uiSystem.state.clipZMax;

        m_graphics->UpdateScene(scene);
        m_graphics->RenderFrame();
    }

} // namespace BimCore