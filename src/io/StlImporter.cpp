// =============================================================================
// BimCore/io/StlImporter.cpp
// =============================================================================
#include "StlImporter.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <filesystem>

namespace BimCore {

    // Standard 50-byte Binary STL Triangle Chunk
    #pragma pack(push, 1)
    struct StlTriangle {
        float normal[3];
        float v0[3];
        float v1[3];
        float v2[3];
        uint16_t attributeByteCount;
    };
    #pragma pack(pop)

    bool StlImporter::Import(const std::string& filepath, std::shared_ptr<SceneModel> targetModel) {
        if (!targetModel) return false;

        std::ifstream file(filepath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "[STL Importer] Failed to open file: " << filepath << "\n";
            return false;
        }

        std::streamsize fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        if (fileSize < 84) {
            std::cerr << "[STL Importer] File is too small to be a valid STL.\n";
            return false;
        }

        // 1. Read 80-byte header
        char header[80];
        file.read(header, 80);

        // 2. Read 4-byte triangle count
        uint32_t numTriangles = 0;
        file.read(reinterpret_cast<char*>(&numTriangles), sizeof(uint32_t));

        // Validate file size to ensure it's a Binary STL
        std::streamsize expectedSize = 84 + (numTriangles * sizeof(StlTriangle));
        if (fileSize < expectedSize) {
            std::cerr << "[STL Importer] File size mismatch. ASCII STLs are not currently supported.\n";
            return false;
        }

        // 3. Read raw triangle data
        std::vector<StlTriangle> triangles(numTriangles);
        file.read(reinterpret_cast<char*>(triangles.data()), numTriangles * sizeof(StlTriangle));
        file.close();

        // 4. Convert to BIMCore RenderMesh
        RenderMesh& mesh = targetModel->GetGeometry();
        
        uint32_t startIndex = static_cast<uint32_t>(mesh.indices.size());
        uint32_t startVertex = static_cast<uint32_t>(mesh.vertices.size());

        mesh.vertices.reserve(mesh.vertices.size() + (numTriangles * 3));
        mesh.indices.reserve(mesh.indices.size() + (numTriangles * 3));

        float minB[3] = { 1e9f, 1e9f, 1e9f };
        float maxB[3] = {-1e9f,-1e9f,-1e9f };

        for (uint32_t i = 0; i < numTriangles; ++i) {
            const StlTriangle& tri = triangles[i];

            // Flat shading STL soup: 3 unique vertices per face
            for (int v = 0; v < 3; ++v) {
                Vertex vertex = {};
                
                // Position
                const float* pos = (v == 0) ? tri.v0 : ((v == 1) ? tri.v1 : tri.v2);
                vertex.position[0] = pos[0];
                vertex.position[1] = pos[1];
                vertex.position[2] = pos[2];

                // Normal (STL provides face normals)
                vertex.normal[0] = tri.normal[0];
                vertex.normal[1] = tri.normal[1];
                vertex.normal[2] = tri.normal[2];

                // Default Color (Light Grey)
                vertex.color[0] = 0.8f;
                vertex.color[1] = 0.8f;
                vertex.color[2] = 0.8f;

                // UVs (STL doesn't support UVs)
                vertex.uv[0] = 0.0f;
                vertex.uv[1] = 0.0f;

                // Update Bounds
                for (int j = 0; j < 3; ++j) {
                    if (pos[j] < minB[j]) minB[j] = pos[j];
                    if (pos[j] > maxB[j]) maxB[j] = pos[j];
                }

                mesh.vertices.push_back(vertex);
                mesh.indices.push_back(startVertex++);
            }
        }

        // 5. Create a single SubMesh to represent the entire STL
        RenderSubMesh subMesh;
        subMesh.guid = "STL_" + std::to_string(std::hash<std::string>{}(filepath));
        subMesh.type = "IfcDiscreteAccessory"; // Map generic mesh to a standard IFC type
        subMesh.startIndex = startIndex;
        subMesh.indexCount = numTriangles * 3;
        subMesh.isTransparent = false;
        subMesh.textureIndex = -1;
        subMesh.center[0] = (minB[0] + maxB[0]) * 0.5f;
        subMesh.center[1] = (minB[1] + maxB[1]) * 0.5f;
        subMesh.center[2] = (minB[2] + maxB[2]) * 0.5f;

        mesh.subMeshes.push_back(subMesh);
        
        // Update global bounds
        for (int j = 0; j < 3; ++j) {
            if (minB[j] < mesh.minBounds[j]) mesh.minBounds[j] = minB[j];
            if (maxB[j] > mesh.maxBounds[j]) mesh.maxBounds[j] = maxB[j];
        }
        
        mesh.originalVertices = mesh.vertices; // Cache for explode tool

        std::cout << "[STL Importer] Successfully loaded " << numTriangles << " triangles.\n";
        return true;
    }

} // namespace BimCore