// =============================================================================
// BimCore/io/GltfExporter.cpp
// =============================================================================
#include "GltfExporter.h"
#include <iostream>
#include <vector>

#define TINYGLTF_NO_INCLUDE_JSON
#include <nlohmann/json.hpp>
#include <tiny_gltf.h>

namespace BimCore {

    void GltfExporter::Export(const std::string& filepath, std::shared_ptr<SceneModel> document) {
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