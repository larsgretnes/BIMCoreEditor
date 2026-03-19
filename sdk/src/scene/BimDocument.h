#pragma once
// =============================================================================
// BimCore/scene/BimDocument.h
// Owns the IFC abstract syntax tree (IfcOpenShell) and the lean GPU render mesh.
// =============================================================================
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <ifcparse/IfcFile.h>

#include "graphics/GraphicsContext.h" // For Vertex

namespace BimCore {

// ---- Render geometry --------------------------------------------------------

struct RenderSubMesh {
    std::string guid;
    std::string type;
    uint32_t    startIndex  = 0;
    uint32_t    indexCount  = 0;
    float       center[3]   = {};
    bool        isTransparent = false;
};

struct RenderMesh {
    std::vector<Vertex>        vertices;
    std::vector<uint32_t>      indices;
    std::vector<RenderSubMesh> subMeshes;

    // Cached original positions for explode-mode reset
    std::vector<Vertex>        originalVertices;

    float minBounds[3] = { 1e9f,  1e9f,  1e9f  };
    float maxBounds[3] = {-1e9f, -1e9f, -1e9f  };
    float center[3]    = { 0.0f,  0.0f,  0.0f  };
};

// ---- Property ledger --------------------------------------------------------

struct PropertyInfo {
    std::string value;
    std::string originalValue;
    bool        isModified = false;
    bool        isDeleted  = false;
};

// ---- Document ---------------------------------------------------------------

class BimDocument {
public:
    BimDocument(std::shared_ptr<IfcParse::IfcFile> database,
                RenderMesh                         geometry,
                const std::string&                 path);

    // Not copyable — this is a heavy owner object
    BimDocument(const BimDocument&)            = delete;
    BimDocument& operator=(const BimDocument&) = delete;

    RenderMesh&                             GetGeometry();
    std::shared_ptr<IfcParse::IfcFile>      GetDatabase();
    std::string                             GetFilePath() const;

    // --- Property ledger API ---
    std::map<std::string, PropertyInfo> GetElementProperties(const std::string& guid);
    bool UpdateElementProperty(const std::string& guid, const std::string& key,   const std::string& value);
    bool DeleteElementProperty(const std::string& guid, const std::string& key);
    bool UndoElementProperty(const std::string& guid,   const std::string& key);

    // --- Geometry editing ---
    bool DeleteElement(const std::string& guid);
    bool UpdateElementColor(const std::string& guid, float r, float g, float b);

    // --- Queries ---
    std::string GetElementNameFast(const std::string& guid);
    bool        HasModifiedProperties(const std::string& guid) const;

    // --- Export prep: flushes ledger into the IFC AST ---
    bool CommitASTChanges();

private:
    void LoadPropertiesFromAST(const std::string& guid);

    std::shared_ptr<IfcParse::IfcFile>                              m_database;
    RenderMesh                                                      m_geometry;
    std::string                                                     m_filePath;
    std::map<std::string, std::map<std::string, PropertyInfo>>      m_propertyCache;
};

} // namespace BimCore
