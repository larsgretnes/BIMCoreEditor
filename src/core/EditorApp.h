// =============================================================================
// BimCore/apps/editor/EditorApp.h
// =============================================================================
#pragma once

#include <memory>
#include <string>
#include <mutex>

#include "core/Core.h"
#include "core/EngineConfig.h"
#include "core/CommandHistory.h"
#include "platform/Window.h"
#include "graphics/GraphicsContext.h"
#include "graphics/Camera.h"
#include "scene/SceneContext.h"
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
        void Render();

    private:
        EngineConfig                        m_config;
        std::unique_ptr<Window>             m_window;
        std::unique_ptr<GraphicsContext>    m_graphics;
        std::unique_ptr<Camera>             m_camera;
        
        CommandHistory                      m_commandHistory;
        AppUI                               m_uiSystem;
        InputController                     m_input;

        SceneContext                        m_sceneContext;

        std::shared_ptr<SceneModel>         m_pendingDoc;
        std::mutex                          m_docMutex;
        LoadState                           m_globalLoadState;

        std::string                         m_currentFilename;
        std::string                         m_currentFileDirectory;
        std::string                         m_safePendingLoadPath;
        std::mutex                          m_loadMutex;

        uint32_t                            m_currentLightMode = 0;
        double                              m_lastTime = 0.0;
    };

} // namespace BimCore