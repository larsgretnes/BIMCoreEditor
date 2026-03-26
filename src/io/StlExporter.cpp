// =============================================================================
// BimCore/io/StlExporter.cpp
// =============================================================================
#include "StlExporter.h"
#include <fstream>
#include <iostream>
#include <glm/glm.hpp>

namespace BimCore {

    #pragma pack(push, 1)
    struct StlTriangle {
        float normal[3];
        float v0[3];
        float v1[3];
        float v2[3];
        uint16_t attributeByteCount;
    };
    #pragma pack(pop)

    bool StlExporter::Export(const std::string& filepath, std::shared_ptr<SceneModel> sourceModel) {
        if (!sourceModel) return false;

        std::ofstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "[STL Exporter] Failed to create file: " << filepath << "\n";
            return false;
        }

        const RenderMesh& mesh = sourceModel->GetGeometry();
        
        // Count valid triangles (excluding hidden/transparent if desired, but we'll export all geometry for now)
        uint32_t numTriangles = static_cast<uint32_t>(mesh.indices.size() / 3);

        // 1. Write 80-byte header
        char header[80] = {0};
        std::string headerText = "Exported by BIMCore Editor v0.3";
        std::copy(headerText.begin(), headerText.end(), header);
        file.write(header, 80);

        // 2. Write triangle count
        file.write(reinterpret_cast<const char*>(&numTriangles), sizeof(uint32_t));

        // 3. Write triangles
        for (size_t i = 0; i < mesh.indices.size(); i += 3) {
            uint32_t i0 = mesh.indices[i];
            uint32_t i1 = mesh.indices[i + 1];
            uint32_t i2 = mesh.indices[i + 2];

            const Vertex& v0 = mesh.vertices[i0];
            const Vertex& v1 = mesh.vertices[i1];
            const Vertex& v2 = mesh.vertices[i2];

            // Calculate exact face normal
            glm::vec3 p0(v0.position[0], v0.position[1], v0.position[2]);
            glm::vec3 p1(v1.position[0], v1.position[1], v1.position[2]);
            glm::vec3 p2(v2.position[0], v2.position[1], v2.position[2]);
            
            glm::vec3 edge1 = p1 - p0;
            glm::vec3 edge2 = p2 - p0;
            glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

            StlTriangle tri = {};
            tri.normal[0] = normal.x;
            tri.normal[1] = normal.y;
            tri.normal[2] = normal.z;

            tri.v0[0] = p0.x; tri.v0[1] = p0.y; tri.v0[2] = p0.z;
            tri.v1[0] = p1.x; tri.v1[1] = p1.y; tri.v1[2] = p1.z;
            tri.v2[0] = p2.x; tri.v2[1] = p2.y; tri.v2[2] = p2.z;
            tri.attributeByteCount = 0;

            file.write(reinterpret_cast<const char*>(&tri), sizeof(StlTriangle));
        }

        file.close();
        std::cout << "[STL Exporter] Successfully exported " << numTriangles << " triangles to " << filepath << "\n";
        return true;
    }

} // namespace BimCore