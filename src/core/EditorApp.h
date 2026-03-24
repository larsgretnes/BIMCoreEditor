// =============================================================================
// BimCore/apps/editor/EditorApp.h
// =============================================================================
#pragma once

#include <memory>
#include <string>
#include <mutex>
#include <vector>
#include <map>

#include "core/Core.h"
#include "core/EngineConfig.h"
#include "core/CommandHistory.h"
#include "platform/Window.h"
#include "graphics/GraphicsContext.h"
#include "graphics/Camera.h"
#include "scene/SceneModel.h"
#include "io/IfcLoader.h"
#include "ui/AppUI.h"
#include "input/InputController.h"

namespace BimCore {

    class EditorApp {
    public:
        EditorApp()  = default;
        ~EditorApp() = default;

        bool Initialize();
        void Run();

    private:
        void HandleAsyncTasks();
        void HandleSaveTask();
        void Update(float deltaTime, bool& triggerFocus);
        void UpdateGeometryOffsets();
        void FocusCameraOnSelection();
        void Render();

        void RebuildMasterMesh();
        // --- NEW: Batch rebuilding helper to stop CPU thrashing ---
        void RebuildRenderBatches(); 

    private:
        // --- Core Engine Systems ---
        EngineConfig                        m_config;
        std::unique_ptr<Window>             m_window;
        std::unique_ptr<GraphicsContext>    m_graphics;
        std::unique_ptr<Camera>             m_camera;
        
        CommandHistory                      m_commandHistory;
        AppUI                               m_uiSystem;
        InputController                     m_input;

        // --- Document State (Multi-Model Support) ---
        std::vector<std::shared_ptr<SceneModel>> m_documents;
        std::shared_ptr<SceneModel>        m_pendingDoc;
        std::mutex                          m_docMutex;
        LoadState                           m_globalLoadState;

        std::vector<Vertex>                 m_masterVertices;
        std::vector<uint32_t>               m_masterIndices;
        bool                                m_triggerRebuild = false;

        // --- NEW: Caching for Performance ---
        float                               m_masterMinBounds[3] = { 1e9f,  1e9f,  1e9f  };
        float                               m_masterMaxBounds[3] = {-1e9f, -1e9f, -1e9f  };
        std::map<int, std::vector<uint32_t>> m_cachedSolidBatches;
        std::map<int, std::vector<uint32_t>> m_cachedTransBatches;
        bool                                m_triggerBatchRebuild = true;

        // --- File Tracking ---
        std::string                         m_currentFilename;
        std::string                         m_currentFileDirectory;
        std::string                         m_safePendingLoadPath;
        std::mutex                          m_loadMutex;

        // --- App State ---
        uint32_t                            m_currentLightMode = 0;
        double                              m_lastTime = 0.0;
    };

} // namespace BimCore