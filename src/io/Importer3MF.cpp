// =============================================================================
// BimCore/io/Importer3MF.cpp
// =============================================================================
#include "Importer3MF.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <map>
#include <string>

// Inkluderer tredjepartsbibliotekene hentet via CMake FetchContent
#include <miniz.h>
#include <pugixml.hpp>

namespace BimCore {

    // Hjelpefunksjon for å beregne kryssprodukt/normaler for 3MF-trekanter
    static void CalculateFaceNormal(const float* v0, const float* v1, const float* v2, float* outNormal) {
        float u[3] = { v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2] };
        float v[3] = { v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2] };

        outNormal[0] = u[1] * v[2] - u[2] * v[1];
        outNormal[1] = u[2] * v[0] - u[0] * v[2];
        outNormal[2] = u[0] * v[1] - u[1] * v[0];

        float length = std::sqrt(outNormal[0] * outNormal[0] + 
                                 outNormal[1] * outNormal[1] + 
                                 outNormal[2] * outNormal[2]);
        if (length > 0.0001f) {
            outNormal[0] /= length;
            outNormal[1] /= length;
            outNormal[2] /= length;
        } else {
            outNormal[0] = 0.0f;
            outNormal[1] = 1.0f;
            outNormal[2] = 0.0f;
        }
    }

    bool Importer3MF::Import(const std::string& filepath, std::shared_ptr<SceneModel> targetModel) {
        if (!targetModel) return false;

        // 1. Åpne 3MF (ZIP) filen ved hjelp av miniz
        mz_zip_archive zip_archive;
        memset(&zip_archive, 0, sizeof(zip_archive));

        if (!mz_zip_reader_init_file(&zip_archive, filepath.c_str(), 0)) {
            std::cerr << "[3MF Importer] Failed to open ZIP archive: " << filepath << "\n";
            return false;
        }

        // 2. Finn hovedmodellen (typisk 3D/3dmodel.model)
        int modelFileIndex = -1;
        for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&zip_archive); i++) {
            mz_zip_archive_file_stat file_stat;
            if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) continue;

            std::string filename = file_stat.m_filename;
            // Et robust søk etter filmerket med .model (kan forbedres ved å parse [Content_Types].xml)
            if (filename.find(".model") != std::string::npos) {
                modelFileIndex = i;
                break;
            }
        }

        if (modelFileIndex == -1) {
            std::cerr << "[3MF Importer] Could not find a .model file inside the 3MF archive.\n";
            mz_zip_reader_end(&zip_archive);
            return false;
        }

        // 3. Pakk ut XML-dataene til minnet
        size_t uncompressed_size = 0;
        void* pUncompressedData = mz_zip_reader_extract_to_heap(&zip_archive, modelFileIndex, &uncompressed_size, 0);
        mz_zip_reader_end(&zip_archive);

        if (!pUncompressedData) {
            std::cerr << "[3MF Importer] Failed to extract the .model file from the archive.\n";
            return false;
        }

        // 4. Parse XML med pugixml
        pugi::xml_document doc;
        pugi::xml_parse_result result = doc.load_buffer(pUncompressedData, uncompressed_size);
        free(pUncompressedData); // Frigi minnet fra miniz umiddelbart

        if (!result) {
            std::cerr << "[3MF Importer] XML Parsing failed: " << result.description() << "\n";
            return false;
        }

        auto& geom = targetModel->GetGeometry();
        pugi::xml_node modelNode = doc.child("model");
        pugi::xml_node resourcesNode = modelNode.child("resources");
        pugi::xml_node buildNode = modelNode.child("build");

        if (!resourcesNode || !buildNode) {
            std::cerr << "[3MF Importer] Invalid 3MF structure: missing <resources> or <build> tags.\n";
            return false;
        }

        // Vi samler alle mesh-dataene midlertidig, da 3MF kan ha bygge-elementer (items) 
        // som refererer til objekter via ID.
        std::map<std::string, pugi::xml_node> objectMap;
        for (pugi::xml_node objectNode = resourcesNode.child("object"); objectNode; objectNode = objectNode.next_sibling("object")) {
            std::string id = objectNode.attribute("id").value();
            objectMap[id] = objectNode;
        }

        uint32_t totalTrianglesImported = 0;

        // 5. Gå gjennom "build" elementene for å finne ut hva som faktisk skal renderes
        for (pugi::xml_node itemNode = buildNode.child("item"); itemNode; itemNode = itemNode.next_sibling("item")) {
            std::string objectId = itemNode.attribute("objectid").value();
            
            if (objectMap.find(objectId) == objectMap.end()) continue;
            pugi::xml_node objectNode = objectMap[objectId];
            pugi::xml_node meshNode = objectNode.child("mesh");
            
            if (!meshNode) continue; // Hopp over hvis det ikke er en mesh (f.eks. komponenter)

            uint32_t indexOffset = static_cast<uint32_t>(geom.indices.size());
            uint32_t vertexOffset = static_cast<uint32_t>(geom.vertices.size());

            // --- Parse Vertices ---
            pugi::xml_node verticesNode = meshNode.child("vertices");
            std::vector<Vertex> tempVertices;
            
            for (pugi::xml_node vNode = verticesNode.child("vertex"); vNode; vNode = vNode.next_sibling("vertex")) {
                Vertex vert = {};
                vert.position[0] = vNode.attribute("x").as_float();
                vert.position[1] = vNode.attribute("y").as_float();
                vert.position[2] = vNode.attribute("z").as_float();
                
                // Standardfarge hvis materialer ikke er spesifisert (hvit/lysegrå)
                vert.color[0] = 0.9f;
                vert.color[1] = 0.9f;
                vert.color[2] = 0.9f;
                
                vert.uv[0] = 0.0f;
                vert.uv[1] = 0.0f;

                tempVertices.push_back(vert);
            }

            // --- Parse Triangles & Beregn normaler ---
            pugi::xml_node trianglesNode = meshNode.child("triangles");
            uint32_t localTriangleCount = 0;

            for (pugi::xml_node tNode = trianglesNode.child("triangle"); tNode; tNode = tNode.next_sibling("triangle")) {
                uint32_t v1 = tNode.attribute("v1").as_uint();
                uint32_t v2 = tNode.attribute("v2").as_uint();
                uint32_t v3 = tNode.attribute("v3").as_uint();

                if (v1 >= tempVertices.size() || v2 >= tempVertices.size() || v3 >= tempVertices.size()) {
                    continue; // Sikkerhetsjekk mot korrupte indekser
                }

                // Beregn flate-normal for trekanten
                float normal[3];
                CalculateFaceNormal(tempVertices[v1].position, tempVertices[v2].position, tempVertices[v3].position, normal);

                // Tilegn normalen til de tre toppunktene i denne flaten.
                // Merk: Hvis vi vil ha glatte overflater senere, kan vi akkumulere normaler per verteks her i stedet.
                for (int i = 0; i < 3; ++i) {
                    tempVertices[v1].normal[i] = normal[i];
                    tempVertices[v2].normal[i] = normal[i];
                    tempVertices[v3].normal[i] = normal[i];
                }

                geom.indices.push_back(vertexOffset + v1);
                geom.indices.push_back(vertexOffset + v2);
                geom.indices.push_back(vertexOffset + v3);
                
                localTriangleCount++;
                totalTrianglesImported++;
            }

            // Legg de ferdige toppunktene inn i den globale geometrien
            geom.vertices.insert(geom.vertices.end(), tempVertices.begin(), tempVertices.end());

            // 6. Opprett RenderSubMesh
            RenderSubMesh sub;
            sub.guid = "3MF_" + objectId + "_" + itemNode.attribute("transform").value(); // Unik ID
            sub.type = "IfcDiscreteAccessory"; 
            sub.startIndex = indexOffset;
            sub.indexCount = localTriangleCount * 3;
            sub.textureIndex = -1; // 3MF teksturering krever utvidelser, hopper over i v1
            sub.isTransparent = false;
            
            geom.subMeshes.push_back(sub);
        }

        // 7. Oppdater Bounding Box og Senter
        if (!geom.vertices.empty()) {
            for (int j = 0; j < 3; ++j) {
                geom.minBounds[j] = 1e9f;
                geom.maxBounds[j] = -1e9f;
            }
            for (const auto& v : geom.vertices) {
                for (int j = 0; j < 3; ++j) {
                    if (v.position[j] < geom.minBounds[j]) geom.minBounds[j] = v.position[j];
                    if (v.position[j] > geom.maxBounds[j]) geom.maxBounds[j] = v.position[j];
                }
            }
            geom.center[0] = (geom.minBounds[0] + geom.maxBounds[0]) * 0.5f;
            geom.center[1] = (geom.minBounds[1] + geom.maxBounds[1]) * 0.5f;
            geom.center[2] = (geom.minBounds[2] + geom.maxBounds[2]) * 0.5f;

            geom.originalVertices = geom.vertices; // For explode/transform verktøyet
        }

        std::cout << "[3MF Importer] Successfully imported " << totalTrianglesImported << " triangles.\n";
        return true;
    }

} // namespace BimCore