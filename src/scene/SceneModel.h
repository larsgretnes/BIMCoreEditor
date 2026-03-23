// =============================================================================
// BimCore/scene/SceneModel.h
// =============================================================================
#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <ifcparse/IfcFile.h>

#include "graphics/GraphicsContext.h"

namespace BimCore {

    struct PropertyInfo {
        std::string value;
        std::string originalValue;
        bool isModified = false;
        bool isDeleted = false;
    };

    struct TextureData {
        uint32_t width;
        uint32_t height;
        uint32_t channels;
        std::vector<uint8_t> pixels;
        std::string name;
    };

    struct RenderSubMesh {
        std::string guid;
        std::string type;
        uint32_t    startIndex  = 0;
        uint32_t    indexCount  = 0;
        float       center[3]   = {};
        bool        isTransparent = false;
        int         textureIndex = -1;

        uint32_t    globalStartIndex = 0;
        int         globalTextureIndex = -1;
    };

    struct RenderMesh {
        std::vector<Vertex>        vertices;
        std::vector<uint32_t>      indices;
        std::vector<RenderSubMesh> subMeshes;
        std::vector<TextureData>   textures; 

        std::vector<Vertex>        originalVertices;

        float minBounds[3] = { 1e9f,  1e9f,  1e9f  };
        float maxBounds[3] = {-1e9f, -1e9f, -1e9f  };
        float center[3]    = { 0.0f,  0.0f,  0.0f  };
    };

    class SceneModel {
    public:
        SceneModel(std::shared_ptr<IfcParse::IfcFile> database,
                    RenderMesh                         geometry,
                    const std::string&                 path);

        SceneModel(const SceneModel&)            = delete;
        SceneModel& operator=(const SceneModel&) = delete;

        RenderMesh&                             GetGeometry();
        std::shared_ptr<IfcParse::IfcFile>      GetDatabase();
        std::string                             GetFilePath() const;

        std::map<std::string, PropertyInfo> GetElementProperties(const std::string& guid);
        bool UpdateElementProperty(const std::string& guid, const std::string& key, const std::string& value);
        bool DeleteElementProperty(const std::string& guid, const std::string& key);
        bool UndoElementProperty(const std::string& guid, const std::string& key);

        bool DeleteElement(const std::string& guid);
        bool UpdateElementColor(const std::string& guid, float r, float g, float b);

        std::string GetElementNameFast(const std::string& guid);
        bool        HasModifiedProperties(const std::string& guid) const;

        bool CommitASTChanges();
        
        void ApplyTransform(const glm::mat4& matrix);

        void SetHierarchy(const std::unordered_map<std::string, std::string>& childToParent,
                          const std::unordered_map<std::string, std::vector<std::string>>& parentToChildren) {
            m_childToParent = childToParent;
            m_parentToChildren = parentToChildren;
        }

        std::string GetParent(const std::string& guid) const {
            auto it = m_childToParent.find(guid);
            return it != m_childToParent.end() ? it->second : "";
        }

        std::vector<std::string> GetChildren(const std::string& guid) const {
            auto it = m_parentToChildren.find(guid);
            return it != m_parentToChildren.end() ? it->second : std::vector<std::string>();
        }

        bool IsAssembly(const std::string& guid) const {
            return m_parentToChildren.find(guid) != m_parentToChildren.end();
        }

        bool IsHidden() const { return m_isHidden; }
        void SetHidden(bool hidden) { m_isHidden = hidden; }

        // --- NEW: UI Group Caching ---
        const std::map<std::string, std::vector<uint32_t>>& GetUIGroups() const { return m_uiGroups; }
        void BuildUIGroups();

    private:
        void LoadPropertiesFromAST(const std::string& guid);

        std::shared_ptr<IfcParse::IfcFile>                              m_database;
        RenderMesh                                                      m_geometry;
        std::string                                                     m_filePath;
        std::map<std::string, std::map<std::string, PropertyInfo>>      m_propertyCache;
        std::unordered_map<std::string, std::string>              m_childToParent;
        std::unordered_map<std::string, std::vector<std::string>> m_parentToChildren;

        std::map<std::string, std::vector<uint32_t>>              m_uiGroups;
        bool m_isHidden = false;
    };

} // namespace BimCore