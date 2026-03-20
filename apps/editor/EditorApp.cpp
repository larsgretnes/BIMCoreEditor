// =============================================================================
// BimCore/apps/editor/EditorApp.cpp
// =============================================================================
#include "EditorApp.h"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <iostream>
#include <algorithm>
#include <thread>

#include "scene/IfcExporter.h"
#include "platform/portable-file-dialogs.h"

namespace BimCore {

    struct SelectionBounds {
        glm::vec3 min { kFloatMax, kFloatMax, kFloatMax };
        glm::vec3 max { kFloatMin, kFloatMin, kFloatMin };
        bool      valid = false;
    };

    static SelectionBounds ComputeSelectionBounds(const std::vector<SelectedObject>& objects, const RenderMesh& mesh) {
        SelectionBounds b;
        for (const auto& obj : objects) {
            for (uint32_t i = 0; i < obj.indexCount; ++i) {
                const uint32_t vi = mesh.indices[obj.startIndex + i];
                const float* p  = mesh.vertices[vi].position;
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

        m_window = std::make_unique<Window>(m_config.WindowWidth, m_config.WindowHeight, "BIMCore Editor v0.1");
        m_graphics = std::make_unique<GraphicsContext>(m_window->GetNativeWindow(), m_config.WindowWidth, m_config.WindowHeight);
        m_graphics->InitImGui(m_window->GetNativeWindow());

        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->AddFontDefault();
        static const ImWchar icons_ranges[] = { 0xe000, 0xf8ff, 0 };
        ImFontConfig icons_config;
        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        io.Fonts->AddFontFromFileTTF("fa-solid-900.ttf", 14.0f, &icons_config, icons_ranges);

        m_camera = std::make_unique<Camera>((float)m_config.WindowWidth / (float)m_config.WindowHeight);
        m_uiSystem.state.loadState = &m_globalLoadState;

        if (!m_config.AutoLoadPath.empty()) {
            std::ifstream devFile(m_config.AutoLoadPath);
            if (devFile.good()) {
                devFile.close();
                std::lock_guard<std::mutex> lock(m_loadMutex);
                m_safePendingLoadPath = m_config.AutoLoadPath;
            }
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

            if (m_uiSystem.state.triggerResetCamera && m_document) {
                m_uiSystem.state.triggerResetCamera = false;
                auto& geom = m_document->GetGeometry();
                m_camera->FocusOn(glm::vec3(geom.center[0], geom.center[1], geom.center[2]), 50.0f);
            }

            m_uiSystem.NewFrame();
            bool triggerFocus = false;

            m_uiSystem.Render(m_uiSystem.state, *m_graphics, m_document, *m_camera, m_config.MaxExplodeFactor, triggerFocus, m_input.IsFlightMode());

            if (m_uiSystem.state.triggerLoad) {
                m_uiSystem.state.triggerLoad = false;
                auto fileDialog = pfd::open_file("Select IFC", m_currentFileDirectory, { "IFC Files", "*.ifc" });
                auto files = fileDialog.result();
                if (!files.empty()) {
                    std::lock_guard<std::mutex> lock(m_loadMutex);
                    m_safePendingLoadPath = files[0];
                }
            }

            HandleSaveTask();
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
                m_document = m_pendingDoc;
                m_pendingDoc = nullptr;
                auto& geom = m_document->GetGeometry();

                m_graphics->UploadMesh(geom.vertices, geom.indices);
                m_globalLoadState.isLoaded.store(true);
                m_camera->FocusOn(glm::vec3(geom.center[0], geom.center[1], geom.center[2]), 50.0f);

                std::string title = "BIMCore Editor v0.1 - " + m_currentFilename;
                glfwSetWindowTitle(m_window->GetNativeWindow(), title.c_str());

                m_uiSystem.state.explodeFactor = 0.0f;
                m_uiSystem.state.updateGeometry = true;
                m_uiSystem.state.selectionChanged = false;

                m_uiSystem.state.clipX = geom.maxBounds[0] + 0.1f;
                m_uiSystem.state.clipY = geom.maxBounds[1] + 0.1f;
                m_uiSystem.state.clipZ = geom.maxBounds[2] + 0.1f;

                memset(m_uiSystem.state.globalSearchBuf, 0, sizeof(m_uiSystem.state.globalSearchBuf));
                memset(m_uiSystem.state.localSearchBuf, 0, sizeof(m_uiSystem.state.localSearchBuf));

                m_uiSystem.state.originalProperties.clear();
                m_uiSystem.state.deletedProperties.clear();
                m_uiSystem.state.deletedObjects.clear();
                m_uiSystem.state.hiddenObjects.clear();
                m_uiSystem.state.objects.clear();
                m_uiSystem.state.searchResults.clear();
                m_uiSystem.state.isSearchActive = false;
                m_uiSystem.state.cachedGroups.clear();
                m_uiSystem.state.cachedNames.clear();
                m_uiSystem.state.groupsBuilt = false;
                m_uiSystem.state.hiddenStateChanged = true;
            }
        }
    }

    void EditorApp::HandleSaveTask() {
        if (!m_uiSystem.state.triggerSave || !m_document) return;

        m_uiSystem.state.triggerSave = false;

        std::string defaultSavePath = m_currentFileDirectory + m_currentFilename;
        auto saveDialog = pfd::save_file("Save IFC As", defaultSavePath, { "IFC Files", "*.ifc" });
        std::string savePath = saveDialog.result();

        if (!savePath.empty()) {
            for (const auto& guid : m_uiSystem.state.deletedObjects) {
                m_document->DeleteElement(guid);
                m_uiSystem.state.hiddenObjects.erase(guid);
            }

            m_document->CommitASTChanges();

            std::thread([this, savePath, doc = m_document]() {
                bool success = IfcExporter::ExportIFC(doc, savePath, &m_globalLoadState);
                if (success) std::cout << "[BIMCore] File saved successfully to " << savePath << "\n";
            }).detach();

                m_uiSystem.state.originalProperties.clear();
                m_uiSystem.state.deletedProperties.clear();
                m_uiSystem.state.deletedObjects.clear();
                m_uiSystem.state.objects.clear();
                m_uiSystem.state.cachedNames.clear();
                m_uiSystem.state.groupsBuilt = false;

                size_t pos = savePath.find_last_of("/\\");
                m_currentFilename = (pos != std::string::npos) ? savePath.substr(pos + 1) : savePath;
                m_currentFileDirectory = (pos != std::string::npos) ? savePath.substr(0, pos + 1) : "";

                std::string title = "BIMCore Editor v0.1 - " + m_currentFilename;
                glfwSetWindowTitle(m_window->GetNativeWindow(), title.c_str());
        }
    }

    void EditorApp::Update(float deltaTime, bool& triggerFocus) {
        if (!m_document) return;

        m_input.Update(*m_window, *m_camera, m_document, m_uiSystem.state, m_config, deltaTime, m_currentLightMode, triggerFocus);
        m_camera->Update(deltaTime);

        // --- NEW: Orbit around selection pivot ---
        if (m_uiSystem.state.selectionChanged) {
            m_uiSystem.state.selectionChanged = false;
            if (!m_uiSystem.state.objects.empty()) {
                auto b = ComputeSelectionBounds(m_uiSystem.state.objects, m_document->GetGeometry());
                if (b.valid) {
                    m_camera->SetPivot((b.min + b.max) * 0.5f);
                }
            }
        }

        if (triggerFocus && !m_uiSystem.state.objects.empty()) {
            auto b = ComputeSelectionBounds(m_uiSystem.state.objects, m_document->GetGeometry());
            if (b.valid) m_camera->FocusOn((b.min + b.max) * 0.5f, glm::length(b.max - b.min) * 0.5f);
        }

        UpdateGeometryOffsets();
    }

    void EditorApp::UpdateGeometryOffsets() {
        if (!m_uiSystem.state.updateGeometry) return;

        auto& mesh = m_document->GetGeometry();

        float oldSMinX = mesh.minBounds[0] - 0.1f; float oldSMaxX = mesh.maxBounds[0] + 0.1f;
        float oldSMinY = mesh.minBounds[1] - 0.1f; float oldSMaxY = mesh.maxBounds[1] + 0.1f;
        float oldSMinZ = mesh.minBounds[2] - 0.1f; float oldSMaxZ = mesh.maxBounds[2] + 0.1f;

        float pctX = std::clamp((m_uiSystem.state.clipX - oldSMinX) / (oldSMaxX - oldSMinX), 0.0f, 1.0f);
        float pctY = std::clamp((m_uiSystem.state.clipY - oldSMinY) / (oldSMaxY - oldSMinY), 0.0f, 1.0f);
        float pctZ = std::clamp((m_uiSystem.state.clipZ - oldSMinZ) / (oldSMaxZ - oldSMinZ), 0.0f, 1.0f);

        mesh.vertices = mesh.originalVertices;

        if (m_uiSystem.state.explodeFactor > 0.01f) {
            glm::vec3 globalCenter(mesh.center[0], mesh.center[1], mesh.center[2]);
            std::vector<bool> shifted(mesh.vertices.size(), false);

            for (const auto& sub : mesh.subMeshes) {
                glm::vec3 subCenter(sub.center[0], sub.center[1], sub.center[2]);
                glm::vec3 dir = subCenter - globalCenter;
                glm::vec3 offset = dir * m_uiSystem.state.explodeFactor;

                for (uint32_t i = 0; i < sub.indexCount; ++i) {
                    uint32_t vIdx = mesh.indices[sub.startIndex + i];
                    if (!shifted[vIdx]) {
                        mesh.vertices[vIdx].position[0] += offset.x;
                        mesh.vertices[vIdx].position[1] += offset.y;
                        mesh.vertices[vIdx].position[2] += offset.z;
                        shifted[vIdx] = true;
                    }
                }
            }
        }

        for (int j=0; j<3; ++j) {
            mesh.minBounds[j] = kFloatMax;
            mesh.maxBounds[j] = kFloatMin;
        }
        for (const auto& v : mesh.vertices) {
            mesh.minBounds[0] = std::min(mesh.minBounds[0], v.position[0]);
            mesh.minBounds[1] = std::min(mesh.minBounds[1], v.position[1]);
            mesh.minBounds[2] = std::min(mesh.minBounds[2], v.position[2]);
            mesh.maxBounds[0] = std::max(mesh.maxBounds[0], v.position[0]);
            mesh.maxBounds[1] = std::max(mesh.maxBounds[1], v.position[1]);
            mesh.maxBounds[2] = std::max(mesh.maxBounds[2], v.position[2]);
        }

        float newSMinX = mesh.minBounds[0] - 0.1f; float newSMaxX = mesh.maxBounds[0] + 0.1f;
        float newSMinY = mesh.minBounds[1] - 0.1f; float newSMaxY = mesh.maxBounds[1] + 0.1f;
        float newSMinZ = mesh.minBounds[2] - 0.1f; float newSMaxZ = mesh.maxBounds[2] + 0.1f;

        m_uiSystem.state.clipX = newSMinX + pctX * (newSMaxX - newSMinX);
        m_uiSystem.state.clipY = newSMinY + pctY * (newSMaxY - newSMinY);
        m_uiSystem.state.clipZ = newSMinZ + pctZ * (newSMaxZ - newSMinZ);

        m_graphics->UpdateGeometry(mesh.vertices);
        m_uiSystem.state.updateGeometry = false;
    }

    void EditorApp::Render() {
        if (!m_document) {
            m_graphics->RenderFrame();
            return;
        }

        std::vector<uint32_t> solid, trans;
        for (const auto& sub : m_document->GetGeometry().subMeshes) {
            if (m_uiSystem.state.hiddenObjects.count(sub.guid)) continue;
            auto& target = sub.isTransparent ? trans : solid;
            for (uint32_t i=0; i<sub.indexCount; ++i) target.push_back(m_document->GetGeometry().indices[sub.startIndex + i]);
        }
        m_graphics->UpdateActiveIndices(solid, trans);

        // --- NEW: Bounding Box Rendering ---
        if (m_uiSystem.state.showBoundingBox && !m_uiSystem.state.objects.empty()) {
            auto b = ComputeSelectionBounds(m_uiSystem.state.objects, m_document->GetGeometry());
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

        m_graphics->SetClippingPlanes(
            m_uiSystem.state.showPlaneX, m_uiSystem.state.clipX, m_uiSystem.state.planeColorX,
            m_uiSystem.state.showPlaneY, m_uiSystem.state.clipY, m_uiSystem.state.planeColorY,
            m_uiSystem.state.showPlaneZ, m_uiSystem.state.clipZ, m_uiSystem.state.planeColorZ,
            glm::vec3(m_document->GetGeometry().minBounds[0], m_document->GetGeometry().minBounds[1], m_document->GetGeometry().minBounds[2]),
                                      glm::vec3(m_document->GetGeometry().maxBounds[0], m_document->GetGeometry().maxBounds[1], m_document->GetGeometry().maxBounds[2])
        );

        SceneUniforms scene{};
        scene.viewProjection = m_camera->GetViewProjectionMatrix();
        scene.lightingMode = m_currentLightMode;
        scene.highlightColor = m_uiSystem.state.color;

        scene.clipActive.x = 1.0f;
        scene.clipActive.y = 1.0f;
        scene.clipActive.z = 1.0f;
        scene.clipDistances.x = m_uiSystem.state.clipX;
        scene.clipDistances.y = m_uiSystem.state.clipY;
        scene.clipDistances.z = m_uiSystem.state.clipZ;

        m_graphics->UpdateScene(scene);
        m_graphics->RenderFrame();
    }

} // namespace BimCore
