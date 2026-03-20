// =============================================================================
// BIMCore Editor — main.cpp
// =============================================================================
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <iostream>
#include <algorithm>
#include <thread>
#include <mutex>
#include <memory>
#include <unordered_set>
#include <fstream>
#include <vector>
#include <glm/glm.hpp>

#include "platform/Window.h"
#include "graphics/GraphicsContext.h"
#include "scene/Camera.h"
#include "scene/IfcLoader.h"
#include "scene/IfcExporter.h"
#include "scene/BimDocument.h"
#include "Core.h"

#include "EngineConfig.h"
#include "AppUI.h"
#include "InputController.h"
#include "platform/portable-file-dialogs.h"

using namespace BimCore;

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

int main() {
    EngineConfig config;
    config.Load();

    Window window(config.WindowWidth, config.WindowHeight, "BIMCore Editor v0.1");
    GraphicsContext graphics(window.GetNativeWindow(), config.WindowWidth, config.WindowHeight);
    graphics.InitImGui(window.GetNativeWindow());

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontDefault();
    static const ImWchar icons_ranges[] = { 0xe000, 0xf8ff, 0 };
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    io.Fonts->AddFontFromFileTTF("fa-solid-900.ttf", 14.0f, &icons_config, icons_ranges);

    Camera camera((float)config.WindowWidth / (float)config.WindowHeight);
    AppUI uiSystem;
    InputController input;
    LoadState globalLoadState;
    uiSystem.state.loadState = &globalLoadState;

    std::shared_ptr<BimDocument> document = nullptr;
    std::shared_ptr<BimDocument> pendingDoc = nullptr;
    std::mutex docMutex;

    // Path Tracking
    std::string currentFilename = "";
    std::string currentFileDirectory = "";

    std::string safePendingLoadPath = "";
    std::mutex loadMutex;

    uint32_t currentLightMode = 0;
    double lastTime = glfwGetTime();

    if (!config.AutoLoadPath.empty()) {
        std::ifstream devFile(config.AutoLoadPath);
        if (devFile.good()) {
            devFile.close();
            std::lock_guard<std::mutex> lock(loadMutex);
            safePendingLoadPath = config.AutoLoadPath;
        }
    }

    while (!window.ShouldClose()) {
        double now = glfwGetTime();
        float deltaTime = static_cast<float>(now - lastTime);
        lastTime = now;

        window.PollEvents();
        if (window.WasWindowResized()) {
            graphics.Resize(window.GetWidth(), window.GetHeight());
            camera.SetAspectRatio((float)window.GetWidth() / (float)window.GetHeight());
            window.ResetWindowResizedFlag();
        }

        std::string triggerPath = "";
        {
            std::lock_guard<std::mutex> lock(loadMutex);
            if (!safePendingLoadPath.empty()) {
                triggerPath = safePendingLoadPath;
                safePendingLoadPath = "";
            }
        }

        if (!triggerPath.empty()) {
            globalLoadState.Reset();
            size_t pos = triggerPath.find_last_of("/\\");
            currentFilename = (pos != std::string::npos) ? triggerPath.substr(pos + 1) : triggerPath;
            currentFileDirectory = (pos != std::string::npos) ? triggerPath.substr(0, pos + 1) : "";

            std::thread([&, triggerPath]() {
                auto doc = IfcLoader::LoadDocument(triggerPath, &globalLoadState);
                std::lock_guard<std::mutex> lock(docMutex);
                pendingDoc = doc;
            }).detach();
        }

        {
            std::lock_guard<std::mutex> lock(docMutex);
            if (pendingDoc) {
                document = pendingDoc;
                pendingDoc = nullptr;
                graphics.UploadMesh(document->GetGeometry().vertices, document->GetGeometry().indices);
                globalLoadState.isLoaded.store(true);
                camera.FocusOn(glm::vec3(document->GetGeometry().center[0], document->GetGeometry().center[1], document->GetGeometry().center[2]), 50.0f);

                std::string title = "BIMCore Editor v0.1 - " + currentFilename;
                glfwSetWindowTitle(window.GetNativeWindow(), title.c_str());

                uiSystem.state.originalProperties.clear();
                uiSystem.state.deletedProperties.clear();
                uiSystem.state.deletedObjects.clear();
                uiSystem.state.hiddenObjects.clear();
                uiSystem.state.objects.clear();
                uiSystem.state.searchResults.clear();
                uiSystem.state.isSearchActive = false;
                uiSystem.state.cachedGroups.clear();
                uiSystem.state.cachedNames.clear();
                uiSystem.state.groupsBuilt = false;
                uiSystem.state.hiddenStateChanged = true;
            }
        }

        if (uiSystem.state.triggerResetCamera && document) {
            uiSystem.state.triggerResetCamera = false;
            camera.FocusOn(glm::vec3(document->GetGeometry().center[0], document->GetGeometry().center[1], document->GetGeometry().center[2]), 50.0f);
        }

        uiSystem.NewFrame();
        bool triggerFocus = false;

        uiSystem.Render(uiSystem.state, graphics, document, camera, config.MaxExplodeFactor, triggerFocus, input.IsFlightMode());

        if (uiSystem.state.triggerLoad) {
            uiSystem.state.triggerLoad = false;
            auto fileDialog = pfd::open_file("Select IFC", currentFileDirectory, { "IFC Files", "*.ifc" });
            auto files = fileDialog.result();
            if (!files.empty()) {
                std::lock_guard<std::mutex> lock(loadMutex);
                safePendingLoadPath = files[0];
            }
        }

        // =====================================================================
        // SAVE HOOK & IN-MEMORY RESET
        // =====================================================================
        if (uiSystem.state.triggerSave && document) {
            uiSystem.state.triggerSave = false;

            std::string defaultSavePath = currentFileDirectory + currentFilename;
            auto saveDialog = pfd::save_file("Save IFC As", defaultSavePath, { "IFC Files", "*.ifc" });
            std::string savePath = saveDialog.result();

            if (!savePath.empty()) {
                // 1. Permanently remove soft-deleted objects from AST and RenderMesh
                for (const auto& guid : uiSystem.state.deletedObjects) {
                    document->DeleteElement(guid);
                    uiSystem.state.hiddenObjects.erase(guid);
                }

                // 2. Flush all edited properties into the AST & Reset Document Ledger
                document->CommitASTChanges();

                // 3. Export the fresh AST to disk
                std::thread([&, savePath, doc = document]() {
                    bool success = IfcExporter::ExportIFC(doc, savePath, &globalLoadState);
                    if (success) {
                        std::cout << "[BIMCore] File saved successfully to " << savePath << "\n";
                    }
                }).detach();

                // 4. Clear all UI "Edited/Deleted" history trackers instantly
                uiSystem.state.originalProperties.clear();
                uiSystem.state.deletedProperties.clear();
                uiSystem.state.deletedObjects.clear();
                uiSystem.state.objects.clear();
                uiSystem.state.cachedNames.clear();
                uiSystem.state.groupsBuilt = false;

                // Update Window Path
                size_t pos = savePath.find_last_of("/\\");
                currentFilename = (pos != std::string::npos) ? savePath.substr(pos + 1) : savePath;
                currentFileDirectory = (pos != std::string::npos) ? savePath.substr(0, pos + 1) : "";

                std::string title = "BIMCore Editor v0.1 - " + currentFilename;
                glfwSetWindowTitle(window.GetNativeWindow(), title.c_str());
            }
        }

        if (document) {
            input.Update(window, camera, document, uiSystem.state, config, deltaTime, currentLightMode, triggerFocus);
            camera.Update(deltaTime);

            if (triggerFocus && !uiSystem.state.objects.empty()) {
                auto b = ComputeSelectionBounds(uiSystem.state.objects, document->GetGeometry());
                if (b.valid) camera.FocusOn((b.min + b.max) * 0.5f, glm::length(b.max - b.min) * 0.5f);
            }

            // =====================================================================
            // THE EXPLODE MATH (CPU-Side)
            // =====================================================================
            if (uiSystem.state.updateGeometry) {
                auto& mesh = document->GetGeometry();
                
                // 1. Reset mesh back to its original state
                mesh.vertices = mesh.originalVertices;

                // 2. Apply explode offsets if the slider is > 0
                if (uiSystem.state.explodeFactor > 0.01f) {
                    glm::vec3 globalCenter(mesh.center[0], mesh.center[1], mesh.center[2]);
                    
                    // Keep track of shifted vertices so we don't translate shared triangle points twice
                    std::vector<bool> shifted(mesh.vertices.size(), false);

                    for (const auto& sub : mesh.subMeshes) {
                        glm::vec3 subCenter(sub.center[0], sub.center[1], sub.center[2]);
                        glm::vec3 dir = subCenter - globalCenter;
                        glm::vec3 offset = dir * uiSystem.state.explodeFactor;

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

                // 3. Upload the shifted vertices to the GPU
                graphics.UpdateGeometry(mesh.vertices);
                uiSystem.state.updateGeometry = false;
            }

            // --- Visibility and Highlight Updates ---
            std::vector<uint32_t> solid, trans;
            for (const auto& sub : document->GetGeometry().subMeshes) {
                if (uiSystem.state.hiddenObjects.count(sub.guid)) continue;
                auto& target = sub.isTransparent ? trans : solid;
                for (uint32_t i=0; i<sub.indexCount; ++i) target.push_back(document->GetGeometry().indices[sub.startIndex + i]);
            }
            graphics.UpdateActiveIndices(solid, trans);

            if (!uiSystem.state.objects.empty()) {
                std::vector<HighlightRange> ranges;
                for(auto& o : uiSystem.state.objects) ranges.push_back({ o.startIndex, o.indexCount });
                graphics.SetHighlight(true, ranges, uiSystem.state.style);
            } else {
                graphics.SetHighlight(false, {}, 0);
            }
            
            // --- Sync Clipping Planes to GPU ---
            graphics.SetClippingPlanes(
                uiSystem.state.showPlaneX, uiSystem.state.clipX,
                uiSystem.state.showPlaneY, uiSystem.state.clipY,
                uiSystem.state.showPlaneZ, uiSystem.state.clipZ,
                glm::vec3(document->GetGeometry().minBounds[0], document->GetGeometry().minBounds[1], document->GetGeometry().minBounds[2]),
                glm::vec3(document->GetGeometry().maxBounds[0], document->GetGeometry().maxBounds[1], document->GetGeometry().maxBounds[2])
            );
        }

        // =====================================================================
        // UNIFORM DATA UPLOAD
        // =====================================================================
        SceneUniforms scene{};
        scene.viewProjection = camera.GetViewProjectionMatrix();
        scene.lightingMode = currentLightMode;
        scene.highlightColor = uiSystem.state.color;
        
        scene.clipActive.x = uiSystem.state.showPlaneX ? 1.0f : 0.0f;
        scene.clipActive.y = uiSystem.state.showPlaneY ? 1.0f : 0.0f;
        scene.clipActive.z = uiSystem.state.showPlaneZ ? 1.0f : 0.0f;
        scene.clipDistances.x = uiSystem.state.clipX;
        scene.clipDistances.y = uiSystem.state.clipY;
        scene.clipDistances.z = uiSystem.state.clipZ;

        graphics.UpdateScene(scene);
        graphics.RenderFrame();
    }

    graphics.ShutdownImGui();
    return 0;
}