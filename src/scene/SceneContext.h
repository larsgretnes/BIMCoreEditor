// =============================================================================
// BimCore/scene/SceneContext.h
// =============================================================================
#pragma once

#include <memory>
#include <vector>
#include <map>
#include <glm/glm.hpp>

#include "scene/SceneModel.h"
#include "graphics/GraphicsContext.h"
#include "ui/UIState.h"

namespace BimCore {

    struct SelectionBounds {
        glm::vec3 min { 1e9f,  1e9f,  1e9f };
        glm::vec3 max {-1e9f, -1e9f, -1e9f };
        bool      valid = false;
    };

    class SceneContext {
    public:
        SceneContext() = default;
        ~SceneContext() = default;

        void AddDocument(std::shared_ptr<SceneModel> doc);
        const std::vector<std::shared_ptr<SceneModel>>& GetDocuments() const { return m_documents; }
        std::vector<std::shared_ptr<SceneModel>>& GetDocuments() { return m_documents; }
        
        void RebuildMasterMesh(GraphicsContext* graphics, SelectionState& uiState);
        void RebuildRenderBatches(GraphicsContext* graphics, SelectionState& uiState);
        void UpdateGeometryOffsets(GraphicsContext* graphics, SelectionState& uiState, float explodeFactor);

        SelectionBounds ComputeSelectionBounds(const std::vector<SelectedObject>& objects) const;

        float minBounds[3] = { 1e9f,  1e9f,  1e9f  };
        float maxBounds[3] = {-1e9f, -1e9f, -1e9f  };

        bool triggerRebuild = false;
        bool triggerBatchRebuild = true;

    private:
        std::vector<std::shared_ptr<SceneModel>> m_documents;

        std::vector<Vertex> m_masterVertices;
        std::vector<uint32_t> m_masterIndices;

        std::map<int, std::vector<uint32_t>> m_cachedSolidBatches;
        std::map<int, std::vector<uint32_t>> m_cachedTransBatches;
    };

} // namespace BimCore