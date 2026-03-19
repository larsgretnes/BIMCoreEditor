// =============================================================================
// BimCore/apps/editor/AppUI.h
// =============================================================================
#pragma once

#include <vector>
#include <string>
#include <unordered_set>
#include <map>
#include <atomic>
#include <mutex>

#include <glm/glm.hpp>

#include "scene/BimDocument.h"
#include "graphics/GraphicsContext.h"
#include "scene/Camera.h"
#include "scene/IfcLoader.h"
#include "Core.h"

namespace BimCore {

    enum class InteractionTool {
        Select,
        Pan,
        Orbit
    };

    struct SelectedObject {
        std::string                         guid;
        std::string                         type;
        uint32_t                            startIndex;
        uint32_t                            indexCount;
        std::map<std::string, PropertyInfo> properties;
    };

    struct SearchResult {
        uint32_t    subMeshIndex;
        std::string matchType;
        std::string matchKey;
        std::string matchValue;
    };

    struct SelectionState {
        std::vector<SelectedObject>     objects;
        std::unordered_set<std::string> hiddenObjects;
        std::unordered_set<std::string> deletedObjects;

        std::map<std::string, std::map<std::string, std::string>> originalProperties;
        std::map<std::string, std::unordered_set<std::string>>    deletedProperties;

        bool                            hiddenStateChanged = false;
        bool                            updateGeometry     = false;

        bool                            triggerLoad        = false;
        bool                            triggerSave        = false;
        bool                            triggerResetCamera = false; // NEW: Triggers smooth camera return
        bool                            showUI             = true;
        InteractionTool                 activeTool         = InteractionTool::Select;

        float                           explodeFactor      = 0.0f;
        bool                            showPlaneX         = false; float clipX = 0.0f;
        bool                            showPlaneY         = false; float clipY = 0.0f;
        bool                            showPlaneZ         = false; float clipZ = 0.0f;

        glm::vec4                       color              = { 1.0f, 0.5f, 0.0f, 0.5f };
        int                             style              = 0;

        std::string                     activeEditGuid     = "";
        std::string                     activeEditKey      = "";
        char                            editBuffer[256]    = "";
        bool                            focusEditField     = false;

        LoadState* loadState          = nullptr;

        // --- Search & Grouping State ---
        char                            globalSearchBuf[256] = "";
        char                            localSearchBuf[256]  = "";

        bool                            isSearchActive       = false;
        std::atomic<bool>               isSearching          {false};
        std::vector<SearchResult>       searchResults;
        std::mutex                      searchMutex;

        std::map<std::string, std::vector<uint32_t>> cachedGroups;
        std::map<std::string, std::string>           cachedNames; // NEW: Caches 'Name' property for fast tree rendering
        bool                                         groupsBuilt = false;

        // --- Shift-Selection Trackers ---
        int                                          lastClickedVisualIndex = -1;
        std::string                                  lastClickedGroup       = "";
    };

    class AppUI {
    public:
        SelectionState state;

        void NewFrame();
        void Render(SelectionState&              selection,
                    GraphicsContext&             graphics,
                    std::shared_ptr<BimDocument> document,
                    Camera&                      camera,
                    float                        configMaxExplode,
                    bool&                        triggerFocus,
                    bool                         isFlightMode);
    };

} // namespace BimCore
