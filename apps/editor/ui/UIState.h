// =============================================================================
// BimCore/apps/editor/ui/UIState.h
// =============================================================================
#pragma once

#include <vector>
#include <string>
#include <unordered_set>
#include <map>
#include <atomic>
#include <mutex>

#include <glm/glm.hpp>
#include <imgui.h>

#include "scene/BimDocument.h"
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

        bool                            showUI             = true;
        InteractionTool                 activeTool         = InteractionTool::Select;
        bool                            hiddenStateChanged = false;

        bool                            triggerLoad        = false;
        bool                            triggerSave        = false;
        bool                            triggerResetCamera = false;
        LoadState* loadState          = nullptr;

        float                           explodeFactor      = 0.0f;
        bool                            updateGeometry     = false;

        // --- Clipping States ---
        bool                            showPlaneX         = false;
        bool                            showPlaneY         = false;
        bool                            showPlaneZ         = false;
        float                           clipX              = 0.0f;
        float                           clipY              = 0.0f;
        float                           clipZ              = 0.0f;

        float                           planeColorX[3]     = { 1.0f, 0.2f, 0.2f };
        float                           planeColorY[3]     = { 0.2f, 1.0f, 0.2f };
        float                           planeColorZ[3]     = { 0.2f, 0.5f, 1.0f };

        int                             style              = 0;
        glm::vec4                       color              {1.0f, 0.5f, 0.0f, 0.5f};

        std::string                     activeEditGuid     = "";
        std::string                     activeEditKey      = "";
        char                            editBuffer[256]    = "";
        bool                            focusEditField     = false;

        // --- Search & Grouping State ---
        char                            globalSearchBuf[256] = "";
        char                            localSearchBuf[256]  = "";

        bool                            isSearchActive       = false;
        std::atomic<bool>               isSearching          {false};
        std::vector<SearchResult>       searchResults;
        std::mutex                      searchMutex;

        std::map<std::string, std::vector<uint32_t>> cachedGroups;
        std::map<std::string, std::string>           cachedNames;
        bool                                         groupsBuilt = false;

        // --- Shift-Selection Trackers ---
        int                                          lastClickedVisualIndex = -1;
        std::string                                  lastClickedGroup       = "";
        bool selectionChanged  = false;
        bool showBoundingBox   = false;
    };

} // namespace BimCore
