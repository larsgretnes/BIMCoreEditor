// =============================================================================
// BimCore/apps/editor/EditorApp.h
// =============================================================================
#pragma once

#include <memory>
#include <string>
#include <mutex>
#include <vector>

#include "Core.h"
#include "EngineConfig.h"
#include "platform/Window.h"
#include "graphics/GraphicsContext.h"
#include "scene/Camera.h"
#include "scene/BimDocument.h"
#include "scene/IfcLoader.h"
#include "AppUI.h"
#include "InputController.h"

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

    private:
        // --- Core Engine Systems ---
        EngineConfig                        m_config;
        std::unique_ptr<Window>             m_window;
        std::unique_ptr<GraphicsContext>    m_graphics;
        std::unique_ptr<Camera>             m_camera;

        AppUI                               m_uiSystem;
        InputController                     m_input;

        // --- Document State ---
        std::shared_ptr<BimDocument>        m_document;
        std::shared_ptr<BimDocument>        m_pendingDoc;
        std::mutex                          m_docMutex;
        LoadState                           m_globalLoadState;

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
