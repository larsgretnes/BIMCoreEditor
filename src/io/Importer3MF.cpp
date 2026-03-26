// =============================================================================
// BimCore/io/Importer3MF.cpp
// =============================================================================
#include "Importer3MF.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <map>
#include <string>
#include <algorithm>

#include <miniz.h>
#include <pugixml.hpp>

namespace BimCore {

    // Helper function to calculate face normals
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

    // Helper to convert 3MF hex colors (#RRGGBB or #RRGGBBAA) to float arrays
    static void ParseHexColor(const std::string& hex, float* outColor) {
        // Default to light grey if parsing fails
        outColor[0] = 0.8f; outColor[1] = 0.8f; outColor[2] = 0.8f; outColor[3] = 1.0f;
        
        if (hex.empty() || hex[0] != '#') return;
        
        std::string hexStr = hex.substr(1);
        try {
            if (hexStr.length() == 6 || hexStr.length() == 8) {
                outColor[0] = std::stoi(hexStr.substr(0, 2), nullptr, 16) / 255.0f;
                outColor[1] = std::stoi(hexStr.substr(2, 2), nullptr, 16) / 255.0f;
                outColor[2] = std::stoi(hexStr.substr(4, 2), nullptr, 16) / 255.0f;
                if (hexStr.length() == 8) {
                    outColor[3] = std::stoi(hexStr.substr(6, 2), nullptr, 16) / 255.0f;
                }
            }
        } catch (...) {
            // Silently fall back to default color on invalid hex
        }
    }

    bool Importer3MF::Import(const std::string& filepath, std::shared_ptr<SceneModel> targetModel) {
        if (!targetModel) return false;

        mz_zip_archive zip_archive;
        memset(&zip_archive, 0, sizeof(zip_archive));

        if (!mz_zip_reader_init_file(&zip_archive, filepath.c_str(), 0)) {
            std::cerr << "[3MF Importer] Failed to open ZIP archive: " << filepath << "\n";
            return false;
        }

        int modelFileIndex = -1;
        for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&zip_archive); i++) {
            mz_zip_archive_file_stat file_stat;
            if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) continue;

            std::string filename = file_stat.m_filename;
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

        size_t uncompressed_size = 0;
        void* pUncompressedData = mz_zip_reader_extract_to_heap(&zip_archive, modelFileIndex, &uncompressed_size, 0);
        mz_zip_reader_end(&zip_archive);

        if (!pUncompressedData) {
            std::cerr << "[3MF Importer] Failed to extract the .model file from the archive.\n";
            return false;
        }

        pugi::xml_document doc;
        pugi::xml_parse_result result = doc.load_buffer(pUncompressedData, uncompressed_size);
        free(pUncompressedData); 

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

        std::string modelUnit = modelNode.attribute("unit").value();
        if (modelUnit.empty()) modelUnit = "millimeter";

        // --- PRE-PASS: Parse Resource Colors and Materials ---
        struct ColorData { float rgba[4]; };
        std::map<std::string, std::vector<ColorData>> propertyCache;

        // Parse <colorGroup>
        for (pugi::xml_node colorGroup = resourcesNode.child("colorGroup"); colorGroup; colorGroup = colorGroup.next_sibling("colorGroup")) {
            std::string id = colorGroup.attribute("id").value();
            std::vector<ColorData> colors;
            for (pugi::xml_node colorNode = colorGroup.child("color"); colorNode; colorNode = colorNode.next_sibling("color")) {
                ColorData c;
                ParseHexColor(colorNode.attribute("color").value(), c.rgba);
                colors.push_back(c);
            }
            propertyCache[id] = colors;
        }

        // Parse <baseMaterials>
        for (pugi::xml_node baseGroup = resourcesNode.child("baseMaterials"); baseGroup; baseGroup = baseGroup.next_sibling("baseMaterials")) {
            std::string id = baseGroup.attribute("id").value();
            std::vector<ColorData> colors;
            for (pugi::xml_node baseNode = baseGroup.child("base"); baseNode; baseNode = baseNode.next_sibling("base")) {
                ColorData c;
                ParseHexColor(baseNode.attribute("displaycolor").value(), c.rgba);
                colors.push_back(c);
            }
            propertyCache[id] = colors;
        }

        std::map<std::string, pugi::xml_node> objectMap;
        for (pugi::xml_node objectNode = resourcesNode.child("object"); objectNode; objectNode = objectNode.next_sibling("object")) {
            std::string id = objectNode.attribute("id").value();
            objectMap[id] = objectNode;
        }

        uint32_t totalTrianglesImported = 0;

        for (pugi::xml_node itemNode = buildNode.child("item"); itemNode; itemNode = itemNode.next_sibling("item")) {
            std::string objectId = itemNode.attribute("objectid").value();
            
            if (objectMap.find(objectId) == objectMap.end()) continue;
            pugi::xml_node objectNode = objectMap[objectId];
            pugi::xml_node meshNode = objectNode.child("mesh");
            
            if (!meshNode) continue; 

            // Check if the object has a default color/material fallback
            std::string objPid = objectNode.attribute("pid").value();
            std::string objPindexStr = objectNode.attribute("pindex").value();
            int objPindex = objPindexStr.empty() ? 0 : std::stoi(objPindexStr);

            uint32_t indexOffset = static_cast<uint32_t>(geom.indices.size());
            uint32_t vertexOffset = static_cast<uint32_t>(geom.vertices.size());

            // Read the raw pool of vertices
            pugi::xml_node verticesNode = meshNode.child("vertices");
            std::vector<glm::vec3> rawPositions;
            
            for (pugi::xml_node vNode = verticesNode.child("vertex"); vNode; vNode = vNode.next_sibling("vertex")) {
                rawPositions.push_back(glm::vec3(
                    vNode.attribute("x").as_float(),
                    vNode.attribute("y").as_float(),
                    vNode.attribute("z").as_float()
                ));
            }

            pugi::xml_node trianglesNode = meshNode.child("triangles");
            uint32_t localTriangleCount = 0;
            bool hasTransparency = false;

            // Unroll the triangles to give them unique flat normals and per-face colors
            for (pugi::xml_node tNode = trianglesNode.child("triangle"); tNode; tNode = tNode.next_sibling("triangle")) {
                uint32_t v1 = tNode.attribute("v1").as_uint();
                uint32_t v2 = tNode.attribute("v2").as_uint();
                uint32_t v3 = tNode.attribute("v3").as_uint();

                if (v1 >= rawPositions.size() || v2 >= rawPositions.size() || v3 >= rawPositions.size()) continue;

                // 1. Calculate the Normal for this specific triangle
                float normal[3];
                CalculateFaceNormal(&rawPositions[v1].x, &rawPositions[v2].x, &rawPositions[v3].x, normal);

                // 2. Resolve Colors
                float faceColor[4] = { 0.8f, 0.8f, 0.8f, 1.0f }; // Default
                
                std::string triPid = tNode.attribute("pid").value();
                std::string triP1 = tNode.attribute("p1").value();
                
                // Fallback: Triangle -> Object
                std::string activePid = triPid.empty() ? objPid : triPid;
                int activePindex = triP1.empty() ? objPindex : std::stoi(triP1);

                if (!activePid.empty() && propertyCache.count(activePid)) {
                    const auto& colors = propertyCache[activePid];
                    if (activePindex >= 0 && activePindex < (int)colors.size()) {
                        faceColor[0] = colors[activePindex].rgba[0];
                        faceColor[1] = colors[activePindex].rgba[1];
                        faceColor[2] = colors[activePindex].rgba[2];
                        faceColor[3] = colors[activePindex].rgba[3];
                        if (faceColor[3] < 0.99f) hasTransparency = true;
                    }
                }

                // 3. Create unique vertices
                uint32_t currentVertIdx = static_cast<uint32_t>(geom.vertices.size());

                for (int i = 0; i < 3; ++i) {
                    Vertex vert = {};
                    glm::vec3 pos = (i == 0) ? rawPositions[v1] : (i == 1) ? rawPositions[v2] : rawPositions[v3];
                    
                    vert.position[0] = pos.x;
                    vert.position[1] = pos.y;
                    vert.position[2] = pos.z;
                    
                    vert.normal[0] = normal[0];
                    vert.normal[1] = normal[1];
                    vert.normal[2] = normal[2];
                    
                    vert.color[0] = faceColor[0];
                    vert.color[1] = faceColor[1];
                    vert.color[2] = faceColor[2];
                    
                    vert.uv[0] = 0.0f;
                    vert.uv[1] = 0.0f;

                    geom.vertices.push_back(vert);
                    geom.indices.push_back(currentVertIdx + i);
                }
                
                localTriangleCount++;
                totalTrianglesImported++;
            }

            RenderSubMesh sub;
            std::string transformStr = itemNode.attribute("transform").value();
            sub.guid = "3MF_" + objectId + "_" + (transformStr.empty() ? "0" : transformStr); 
            sub.type = "IfcDiscreteAccessory"; 
            sub.startIndex = indexOffset;
            sub.indexCount = localTriangleCount * 3;
            sub.textureIndex = -1; 
            sub.isTransparent = hasTransparency;
            
            geom.subMeshes.push_back(sub);

            // --- Properties / Metadata ---
            std::string objName = objectNode.attribute("name").value();
            if (objName.empty()) objName = "3MF Object " + objectId;
            targetModel->UpdateElementProperty(sub.guid, "Name", objName);

            std::string partNum = objectNode.attribute("partnumber").value();
            if (!partNum.empty()) targetModel->UpdateElementProperty(sub.guid, "Part Number", partNum);

            std::string objType = objectNode.attribute("type").value();
            if (!objType.empty()) targetModel->UpdateElementProperty(sub.guid, "3MF Type", objType);

            targetModel->UpdateElementProperty(sub.guid, "Unit", modelUnit);

            for (pugi::xml_node metaNode = objectNode.child("metadata"); metaNode; metaNode = metaNode.next_sibling("metadata")) {
                std::string metaName = metaNode.attribute("name").value();
                std::string metaValue = metaNode.text().get();
                if (!metaName.empty() && !metaValue.empty()) {
                    targetModel->UpdateElementProperty(sub.guid, metaName, metaValue);
                }
            }
            
            if (!transformStr.empty()) {
                targetModel->UpdateElementProperty(sub.guid, "Transform Matrix", transformStr);
            }
        }

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

            geom.originalVertices = geom.vertices; 
        }

        std::cout << "[3MF Importer] Successfully imported " << totalTrianglesImported << " triangles with colors & metadata.\n";
        return true;
    }

} // namespace BimCore