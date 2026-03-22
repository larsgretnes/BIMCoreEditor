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

    // --- FIXED: Reverted to the core 3 tools. Measure is now an independent mode! ---
    enum class InteractionTool { Select, Pan, Orbit };
    enum class SnapType { None, Vertex, Edge, Face };

    struct Measurement {
        glm::vec3 p1;
        glm::vec3 p2;
    };

    struct Measurement2D {
        float p1[2];
        float p2[2];
        char  text[64];
    };

    struct SnapOverlay2D {
        bool     draw = false;
        SnapType type = SnapType::None;
        float    p[2];
        float    e0[2];
        float    e1[2];
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

        bool                            showPlaneXMin = false, showPlaneXMax = false;
        bool                            showPlaneYMin = false, showPlaneYMax = false;
        bool                            showPlaneZMin = false, showPlaneZMax = false;

        float                           clipXMin = 0.0f, clipXMax = 0.0f;
        float                           clipYMin = 0.0f, clipYMax = 0.0f;
        float                           clipZMin = 0.0f, clipZMax = 0.0f;

        float                           planeColorX[3]     = { 1.0f, 0.2f, 0.2f };
        float                           planeColorY[3]     = { 0.2f, 1.0f, 0.2f };
        float                           planeColorZ[3]     = { 0.2f, 0.5f, 1.0f };

        int                             style              = 0;
        glm::vec4                       color              {1.0f, 0.5f, 0.0f, 0.5f};

        std::string                     activeEditGuid     = "";
        std::string                     activeEditKey      = "";
        char                            editBuffer[256]    = "";
        bool                            focusEditField     = false;

        char                            globalSearchBuf[256] = "";
        char                            localSearchBuf[256]  = "";

        bool                            isSearchActive       = false;
        std::atomic<bool>               isSearching          {false};
        std::vector<SearchResult>       searchResults;
        std::mutex                      searchMutex;

        std::map<std::string, std::vector<uint32_t>> cachedGroups;
        std::map<std::string, std::string>           cachedNames;
        bool                                         groupsBuilt = false;
        bool                                         selectionChanged = false;

        bool                                         showBoundingBox = false;
        bool                                         showOpeningsAndSpaces = false;
        bool                                         selectAssemblies = true;

        int                                          lastClickedVisualIndex = -1;
        std::string                                  lastClickedGroup       = "";

        // --- NEW: Independent Measure Mode Toggle ---
        bool                       measureToolActive = false;

        std::vector<Measurement>   completedMeasurements;
        std::vector<Measurement2D> renderMeasurements;
        SnapOverlay2D              renderSnap;
        Measurement2D              renderActiveLine;
        bool                       drawActiveLine = false;

        bool      isMeasuringActive = false;
        bool      isHoveringGeometry = false;
        glm::vec3 measureStartPoint {0,0,0};

        SnapType  currentSnapType = SnapType::None;
        glm::vec3 currentSnapPoint {0,0,0};
        glm::vec3 currentSnapEdgeV0 {0,0,0};
        glm::vec3 currentSnapEdgeV1 {0,0,0};
    };

} // namespace BimCore
