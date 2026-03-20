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
                auto& geom = document->GetGeometry();
                graphics.UploadMesh(geom.vertices, geom.indices);
                globalLoadState.isLoaded.store(true);
                camera.FocusOn(glm::vec3(geom.center[0], geom.center[1], geom.center[2]), 50.0f);

                std::string title = "BIMCore Editor v0.1 - " + currentFilename;
                glfwSetWindowTitle(window.GetNativeWindow(), title.c_str());

                uiSystem.state.explodeFactor = 0.0f;
                uiSystem.state.updateGeometry = true;

                // Initialize clip distances to safely envelope the whole model by default
                uiSystem.state.clipX = geom.maxBounds[0] + 0.1f;
                uiSystem.state.clipY = geom.maxBounds[1] + 0.1f;
                uiSystem.state.clipZ = geom.maxBounds[2] + 0.1f;

                memset(uiSystem.state.globalSearchBuf, 0, sizeof(uiSystem.state.globalSearchBuf));
                memset(uiSystem.state.localSearchBuf, 0, sizeof(uiSystem.state.localSearchBuf));

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

        if (uiSystem.state.triggerSave && document) {
            uiSystem.state.triggerSave = false;

            std::string defaultSavePath = currentFileDirectory + currentFilename;
            auto saveDialog = pfd::save_file("Save IFC As", defaultSavePath, { "IFC Files", "*.ifc" });
            std::string savePath = saveDialog.result();

            if (!savePath.empty()) {
                for (const auto& guid : uiSystem.state.deletedObjects) {
                    document->DeleteElement(guid);
                    uiSystem.state.hiddenObjects.erase(guid);
                }

                document->CommitASTChanges();

                std::thread([&, savePath, doc = document]() {
                    bool success = IfcExporter::ExportIFC(doc, savePath, &globalLoadState);
                    if (success) std::cout << "[BIMCore] File saved successfully to " << savePath << "\n";
                }).detach();

                    uiSystem.state.originalProperties.clear();
                    uiSystem.state.deletedProperties.clear();
                    uiSystem.state.deletedObjects.clear();
                    uiSystem.state.objects.clear();
                    uiSystem.state.cachedNames.clear();
                    uiSystem.state.groupsBuilt = false;

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
            // UPDATE GEOMETRY & MAP SLIDER PERCENTAGES TO NEW BOUNDS
            // =====================================================================
            if (uiSystem.state.updateGeometry) {
                auto& mesh = document->GetGeometry();

                // 1. Calculate the percentage of where the slider handles were
                float oldSMinX = mesh.minBounds[0] - 0.1f; float oldSMaxX = mesh.maxBounds[0] + 0.1f;
                float oldSMinY = mesh.minBounds[1] - 0.1f; float oldSMaxY = mesh.maxBounds[1] + 0.1f;
                float oldSMinZ = mesh.minBounds[2] - 0.1f; float oldSMaxZ = mesh.maxBounds[2] + 0.1f;

                float pctX = std::clamp((uiSystem.state.clipX - oldSMinX) / (oldSMaxX - oldSMinX), 0.0f, 1.0f);
                float pctY = std::clamp((uiSystem.state.clipY - oldSMinY) / (oldSMaxY - oldSMinY), 0.0f, 1.0f);
                float pctZ = std::clamp((uiSystem.state.clipZ - oldSMinZ) / (oldSMaxZ - oldSMinZ), 0.0f, 1.0f);

                // 2. Perform the CPU-side Explode Math
                mesh.vertices = mesh.originalVertices;

                if (uiSystem.state.explodeFactor > 0.01f) {
                    glm::vec3 globalCenter(mesh.center[0], mesh.center[1], mesh.center[2]);
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

                // 3. Recalculate New Full Bounds!
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

                // 4. Remap sliders dynamically into the newly expanded space!
                float newSMinX = mesh.minBounds[0] - 0.1f; float newSMaxX = mesh.maxBounds[0] + 0.1f;
                float newSMinY = mesh.minBounds[1] - 0.1f; float newSMaxY = mesh.maxBounds[1] + 0.1f;
                float newSMinZ = mesh.minBounds[2] - 0.1f; float newSMaxZ = mesh.maxBounds[2] + 0.1f;

                uiSystem.state.clipX = newSMinX + pctX * (newSMaxX - newSMinX);
                uiSystem.state.clipY = newSMinY + pctY * (newSMaxY - newSMinY);
                uiSystem.state.clipZ = newSMinZ + pctZ * (newSMaxZ - newSMinZ);

                graphics.UpdateGeometry(mesh.vertices);
                uiSystem.state.updateGeometry = false;
            }

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

            // --- DRAW CLIPPING GLASS PLANES ---
            graphics.SetClippingPlanes(
                uiSystem.state.showPlaneX, uiSystem.state.clipX, uiSystem.state.planeColorX,
                uiSystem.state.showPlaneY, uiSystem.state.clipY, uiSystem.state.planeColorY,
                uiSystem.state.showPlaneZ, uiSystem.state.clipZ, uiSystem.state.planeColorZ,
                glm::vec3(document->GetGeometry().minBounds[0], document->GetGeometry().minBounds[1], document->GetGeometry().minBounds[2]),
                                       glm::vec3(document->GetGeometry().maxBounds[0], document->GetGeometry().maxBounds[1], document->GetGeometry().maxBounds[2])
            );
        }

        SceneUniforms scene{};
        scene.viewProjection = camera.GetViewProjectionMatrix();
        scene.lightingMode = currentLightMode;
        scene.highlightColor = uiSystem.state.color;

        // --- SHADER CLIPPING MATH IS ALWAYS ACTIVE ---
        scene.clipActive.x = 1.0f;
        scene.clipActive.y = 1.0f;
        scene.clipActive.z = 1.0f;
        scene.clipDistances.x = uiSystem.state.clipX;
        scene.clipDistances.y = uiSystem.state.clipY;
        scene.clipDistances.z = uiSystem.state.clipZ;

        graphics.UpdateScene(scene);
        graphics.RenderFrame();
    }

    graphics.ShutdownImGui();
    return 0;
}
