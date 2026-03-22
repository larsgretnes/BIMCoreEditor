// =============================================================================
// BimCore/scene/FormatModules.cpp
// =============================================================================
#include "FormatModules.h"
#include <iostream>
#include <algorithm>
#include <vector>

// --- TinyGLTF Implementation Configuration ---
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NO_INCLUDE_JSON
#include <nlohmann/json.hpp>
#include <tiny_gltf.h>

namespace BimCore {

    void BcfImporter::Import(const std::string& filepath, std::shared_ptr<BimDocument> document) {
        std::cout << "[BIMCore] Stub: Preparing to parse BCF XML from " << filepath << "\n";
    }

    // -------------------------------------------------------------------------
    // GLTF / GLB IMPORTER
    // -------------------------------------------------------------------------
    void GltfImporter::Import(const std::string& filepath, std::shared_ptr<BimDocument> document) {
        tinygltf::TinyGLTF loader;
        tinygltf::Model model;
        std::string err, warn;

        bool isGlb = filepath.size() > 4 && filepath.substr(filepath.size() - 4) == ".glb";
        bool success = isGlb ? loader.LoadBinaryFromFile(&model, &err, &warn, filepath)
        : loader.LoadASCIIFromFile(&model, &err, &warn, filepath);

        if (!warn.empty()) std::cout << "[glTF Warn] " << warn << "\n";
        if (!err.empty()) std::cerr << "[glTF Error] " << err << "\n";
        if (!success) return;

        auto& geom = document->GetGeometry();

        std::vector<int> textureMap(model.textures.size(), -1);
        for (size_t i = 0; i < model.textures.size(); ++i) {
            int sourceIdx = model.textures[i].source;
            if (sourceIdx < 0 || sourceIdx >= static_cast<int>(model.images.size())) continue;

            const auto& img = model.images[sourceIdx];

            TextureData tex;
            tex.width = img.width;
            tex.height = img.height;
            tex.name = img.name;

            if (img.component == 3) {
                tex.pixels.resize(img.width * img.height * 4);
                for (size_t p = 0; p < static_cast<size_t>(img.width * img.height); ++p) {
                    tex.pixels[p*4 + 0] = img.image[p*3 + 0];
                    tex.pixels[p*4 + 1] = img.image[p*3 + 1];
                    tex.pixels[p*4 + 2] = img.image[p*3 + 2];
                    tex.pixels[p*4 + 3] = 255;
                }
                tex.channels = 4;
            } else if (img.component == 4) {
                tex.pixels = img.image;
                tex.channels = 4;
            } else {
                continue;
            }

            textureMap[i] = static_cast<int>(geom.textures.size());
            geom.textures.push_back(tex);
        }

        for (size_t i = 0; i < model.meshes.size(); ++i) {
            const auto& mesh = model.meshes[i];
            for (const auto& primitive : mesh.primitives) {
                if (primitive.mode != TINYGLTF_MODE_TRIANGLES) continue;

                uint32_t vertexOffset = static_cast<uint32_t>(geom.vertices.size());
                uint32_t indexOffset = static_cast<uint32_t>(geom.indices.size());

                std::vector<double> baseColor = {1.0, 1.0, 1.0, 1.0};
                int matchedTextureIndex = -1;

                if (primitive.material >= 0) {
                    const auto& mat = model.materials[primitive.material];
                    if (mat.pbrMetallicRoughness.baseColorFactor.size() == 4) {
                        baseColor = mat.pbrMetallicRoughness.baseColorFactor;
                    }
                    if (mat.pbrMetallicRoughness.baseColorTexture.index >= 0) {
                        int gltfTexIdx = mat.pbrMetallicRoughness.baseColorTexture.index;
                        if (gltfTexIdx < static_cast<int>(textureMap.size())) {
                            matchedTextureIndex = textureMap[gltfTexIdx];
                        }
                    }
                }

                if (primitive.attributes.count("POSITION")) {
                    const auto& acc = model.accessors[primitive.attributes.at("POSITION")];
                    const auto& view = model.bufferViews[acc.bufferView];
                    size_t posStride = view.byteStride == 0 ? 12 : view.byteStride;
                    const uint8_t* posBase = model.buffers[view.buffer].data.data() + view.byteOffset + acc.byteOffset;

                    const uint8_t* uvBase = nullptr;
                    size_t uvStride = 8;
                    if (primitive.attributes.count("TEXCOORD_0")) {
                        const auto& uvAcc = model.accessors[primitive.attributes.at("TEXCOORD_0")];
                        const auto& uvView = model.bufferViews[uvAcc.bufferView];
                        uvStride = uvView.byteStride == 0 ? 8 : uvView.byteStride;
                        uvBase = model.buffers[uvView.buffer].data.data() + uvView.byteOffset + uvAcc.byteOffset;
                    }

                    const uint8_t* normBase = nullptr;
                    size_t normStride = 12;
                    if (primitive.attributes.count("NORMAL")) {
                        const auto& normAcc = model.accessors[primitive.attributes.at("NORMAL")];
                        const auto& normView = model.bufferViews[normAcc.bufferView];
                        normStride = normView.byteStride == 0 ? 12 : normView.byteStride;
                        normBase = model.buffers[normView.buffer].data.data() + normView.byteOffset + normAcc.byteOffset;
                    }

                    for (size_t v = 0; v < acc.count; ++v) {
                        Vertex vert = {};

                        const float* p = reinterpret_cast<const float*>(posBase + v * posStride);

                        // --- FIXED: Strict 1:1 Mapping. No rotation! ---
                        vert.position[0] = p[0];
                        vert.position[1] = p[1];
                        vert.position[2] = p[2];

                        if (uvBase) {
                            const float* u = reinterpret_cast<const float*>(uvBase + v * uvStride);
                            vert.uv[0] = u[0];
                            vert.uv[1] = u[1];
                        }

                        if (normBase) {
                            const float* n = reinterpret_cast<const float*>(normBase + v * normStride);
                            // --- FIXED: Strict 1:1 Mapping for Normals ---
                            vert.normal[0] = n[0];
                            vert.normal[1] = n[1];
                            vert.normal[2] = n[2];
                        }

                        vert.color[0] = static_cast<float>(baseColor[0]);
                        vert.color[1] = static_cast<float>(baseColor[1]);
                        vert.color[2] = static_cast<float>(baseColor[2]);
                        geom.vertices.push_back(vert);
                    }
                }

                if (primitive.indices >= 0) {
                    const auto& acc = model.accessors[primitive.indices];
                    const auto& view = model.bufferViews[acc.bufferView];
                    const void* data = &model.buffers[view.buffer].data[view.byteOffset + acc.byteOffset];

                    for (size_t idx = 0; idx < acc.count; ++idx) {
                        uint32_t val;
                        if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                            val = static_cast<const uint16_t*>(data)[idx];
                        } else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                            val = static_cast<const uint8_t*>(data)[idx];
                        } else {
                            val = static_cast<const uint32_t*>(data)[idx];
                        }
                        geom.indices.push_back(vertexOffset + val);
                    }
                }

                RenderSubMesh sub;
                sub.guid = "GLTF_" + std::to_string(i) + "_" + mesh.name;
                sub.type = "ExternalGeometry";
                sub.startIndex = indexOffset;
                sub.indexCount = static_cast<uint32_t>(geom.indices.size() - indexOffset);
                sub.textureIndex = matchedTextureIndex;
                sub.isTransparent = (baseColor[3] < 0.99);
                geom.subMeshes.push_back(sub);
            }
        }

        if (!geom.vertices.empty()) {
            for (int j=0; j<3; ++j) {
                geom.minBounds[j] = 1e9f;
                geom.maxBounds[j] = -1e9f;
            }
            for (const auto& v : geom.vertices) {
                geom.minBounds[0] = std::min(geom.minBounds[0], v.position[0]);
                geom.minBounds[1] = std::min(geom.minBounds[1], v.position[1]);
                geom.minBounds[2] = std::min(geom.minBounds[2], v.position[2]);
                geom.maxBounds[0] = std::max(geom.maxBounds[0], v.position[0]);
                geom.maxBounds[1] = std::max(geom.maxBounds[1], v.position[1]);
                geom.maxBounds[2] = std::max(geom.maxBounds[2], v.position[2]);
            }
            geom.center[0] = (geom.minBounds[0] + geom.maxBounds[0]) * 0.5f;
            geom.center[1] = (geom.minBounds[1] + geom.maxBounds[1]) * 0.5f;
            geom.center[2] = (geom.minBounds[2] + geom.maxBounds[2]) * 0.5f;

            geom.originalVertices = geom.vertices;
        }

        std::cout << "[BIMCore] Imported " << geom.textures.size() << " textures from glTF.\n";
    }

    // -------------------------------------------------------------------------
    // GLTF / GLB EXPORTER
    // -------------------------------------------------------------------------
    void GltfExporter::Export(const std::string& filepath, std::shared_ptr<BimDocument> document) {
        if (!document) return;
        const auto& geom = document->GetGeometry();

        tinygltf::Model model;
        tinygltf::Mesh mesh;
        mesh.name = "BIMCore_Export";

        uint32_t totalIndices = 0;
        for (const auto& sub : geom.subMeshes) totalIndices += sub.indexCount;

        std::vector<float> posData; posData.reserve(geom.vertices.size() * 3);
        std::vector<float> norData; norData.reserve(geom.vertices.size() * 3);
        std::vector<float> colData; colData.reserve(geom.vertices.size() * 3);
        std::vector<float> uvData;  uvData.reserve(geom.vertices.size() * 2);

        for (const auto& v : geom.vertices) {
            // --- FIXED: Strict 1:1 Mapping for export too ---
            posData.push_back(v.position[0]);
            posData.push_back(v.position[1]);
            posData.push_back(v.position[2]);

            norData.push_back(v.normal[0]);
            norData.push_back(v.normal[1]);
            norData.push_back(v.normal[2]);

            colData.push_back(v.color[0]);
            colData.push_back(v.color[1]);
            colData.push_back(v.color[2]);

            uvData.push_back(v.uv[0]);
            uvData.push_back(v.uv[1]);
        }

        std::vector<uint32_t> indData; indData.reserve(totalIndices);
        for (const auto& sub : geom.subMeshes) {
            for (uint32_t i = 0; i < sub.indexCount; ++i) indData.push_back(geom.indices[sub.startIndex + i]);
        }

        auto addBufferView = [&](const void* data, size_t size) -> int {
            tinygltf::BufferView bv;
            bv.buffer = 0;
            bv.byteOffset = model.buffers.empty() ? 0 : model.buffers[0].data.size();
            bv.byteLength = size;

            if (model.buffers.empty()) model.buffers.push_back(tinygltf::Buffer());
            auto& buf = model.buffers[0];
            const uint8_t* p = static_cast<const uint8_t*>(data);
            buf.data.insert(buf.data.end(), p, p + size);

            model.bufferViews.push_back(bv);
            return static_cast<int>(model.bufferViews.size() - 1);
        };

        int posView = addBufferView(posData.data(), posData.size() * sizeof(float));
        int norView = addBufferView(norData.data(), norData.size() * sizeof(float));
        int colView = addBufferView(colData.data(), colData.size() * sizeof(float));
        int uvView  = addBufferView(uvData.data(),  uvData.size()  * sizeof(float));
        int indView = addBufferView(indData.data(), indData.size() * sizeof(uint32_t));

        auto addAccessor = [&](int view, int type, int compType, size_t count) -> int {
            tinygltf::Accessor acc;
            acc.bufferView = view;
            acc.byteOffset = 0;
            acc.componentType = compType;
            acc.count = count;
            acc.type = type;
            model.accessors.push_back(acc);
            return static_cast<int>(model.accessors.size() - 1);
        };

        int posAcc = addAccessor(posView, TINYGLTF_TYPE_VEC3, TINYGLTF_COMPONENT_TYPE_FLOAT, geom.vertices.size());
        int norAcc = addAccessor(norView, TINYGLTF_TYPE_VEC3, TINYGLTF_COMPONENT_TYPE_FLOAT, geom.vertices.size());
        int colAcc = addAccessor(colView, TINYGLTF_TYPE_VEC3, TINYGLTF_COMPONENT_TYPE_FLOAT, geom.vertices.size());
        int uvAcc  = addAccessor(uvView,  TINYGLTF_TYPE_VEC2, TINYGLTF_COMPONENT_TYPE_FLOAT, geom.vertices.size());
        int indAcc = addAccessor(indView, TINYGLTF_TYPE_SCALAR, TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT, indData.size());

        tinygltf::Primitive prim;
        prim.attributes["POSITION"]   = posAcc;
        prim.attributes["NORMAL"]     = norAcc;
        prim.attributes["COLOR_0"]    = colAcc;
        prim.attributes["TEXCOORD_0"] = uvAcc;
        prim.indices = indAcc;
        prim.mode = TINYGLTF_MODE_TRIANGLES;
        mesh.primitives.push_back(prim);

        model.meshes.push_back(mesh);

        tinygltf::Node node;
        node.mesh = 0;
        model.nodes.push_back(node);

        tinygltf::Scene scene;
        scene.nodes.push_back(0);
        model.scenes.push_back(scene);
        model.defaultScene = 0;

        tinygltf::Asset asset;
        asset.version = "2.0";
        asset.generator = "BIMCore Editor v0.2";
        model.asset = asset;

        tinygltf::TinyGLTF gltf;
        bool isGlb = filepath.size() > 4 && filepath.substr(filepath.size() - 4) == ".glb";
        bool success = isGlb ? gltf.WriteGltfSceneToFile(&model, filepath, true, true, true, true)
        : gltf.WriteGltfSceneToFile(&model, filepath, true, true, true, false);

        if (success) std::cout << "[BIMCore] Exported glTF to " << filepath << "\n";
    }

} // namespace BimCore
