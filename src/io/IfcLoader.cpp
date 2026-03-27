// =============================================================================
// BimCore/scene/IfcLoader.cpp
// =============================================================================
#include "IfcLoader.h"
#include "core/Core.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <future>
#include <queue>
#include <deque>
#include <condition_variable>
#include <functional>
#include <cmath>
#include <chrono>
#include <array>
#include <filesystem>
#include <execution> 

#include <ifcparse/IfcFile.h>
#include <ifcgeom/Iterator.h>
#include <ifcgeom/kernels/opencascade/OpenCascadeKernel.h>
#include <ifcparse/Ifc2x3.h>
#include <ifcparse/Ifc4.h>

namespace BimCore {

namespace {
    class ThreadPool {
    public:
        ThreadPool(size_t threads, size_t max_queue) 
            : stop(false), max_queue_size(max_queue) {
            for (size_t i = 0; i < threads; ++i) {
                workers.emplace_back([this] {
                    for (;;) {
                        std::function<void()> task;
                        {
                            std::unique_lock<std::mutex> lock(this->queue_mutex);
                            this->condition_consumers.wait(lock, [this] { 
                                return this->stop || !this->tasks.empty(); 
                            });
                            if (this->stop && this->tasks.empty()) return;
                            task = std::move(this->tasks.front());
                            this->tasks.pop();
                        }
                        this->condition_producers.notify_one(); 
                        task();
                    }
                });
            }
        }

        template<class F>
        auto Enqueue(F&& f) -> std::future<typename std::invoke_result_t<F>> {
            using return_type = typename std::invoke_result_t<F>;
            auto task = std::make_shared<std::packaged_task<return_type()>>(std::forward<F>(f));
            std::future<return_type> res = task->get_future();
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                condition_producers.wait(lock, [this] { 
                    return tasks.size() < max_queue_size || stop; 
                });
                if (stop) throw std::runtime_error("Enqueue on stopped ThreadPool");
                tasks.emplace([task]() { (*task)(); });
            }
            condition_consumers.notify_one();
            return res;
        }

        ~ThreadPool() {
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                stop = true;
            }
            condition_consumers.notify_all();
            condition_producers.notify_all();
            for (std::thread& worker : workers) worker.join();
        }

    private:
        std::vector<std::thread> workers;
        std::queue<std::function<void()>> tasks;
        std::mutex queue_mutex;
        std::condition_variable condition_consumers;
        std::condition_variable condition_producers;
        size_t max_queue_size;
        bool stop;
    };

    struct MeshChunk {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<RenderSubMesh> subMeshes;
        std::vector<BVHNode> bvhNodes;
        float minB[3] = { kFloatMax, kFloatMax, kFloatMax };
        float maxB[3] = { kFloatMin, kFloatMin, kFloatMin };
    };

    // --- CACHING HELPERS ---
    const char BIMCACHE_MAGIC[8] = "BIMCACH";
    const uint32_t BIMCACHE_VERSION = 1;

    void WriteString(std::ofstream& out, const std::string& str) {
        size_t len = str.size();
        out.write(reinterpret_cast<const char*>(&len), sizeof(len));
        if (len > 0) out.write(str.data(), len);
    }

    void ReadString(std::ifstream& in, std::string& str) {
        size_t len = 0;
        in.read(reinterpret_cast<char*>(&len), sizeof(len));
        str.resize(len);
        if (len > 0) in.read(str.data(), len);
    }

    template<typename T>
    void WriteVector(std::ofstream& out, const std::vector<T>& vec) {
        size_t count = vec.size();
        out.write(reinterpret_cast<const char*>(&count), sizeof(count));
        if (count > 0) out.write(reinterpret_cast<const char*>(vec.data()), count * sizeof(T));
    }

    template<typename T>
    void ReadVector(std::ifstream& in, std::vector<T>& vec) {
        size_t count = 0;
        in.read(reinterpret_cast<char*>(&count), sizeof(count));
        vec.resize(count);
        if (count > 0) in.read(reinterpret_cast<char*>(vec.data()), count * sizeof(T));
    }
}

std::shared_ptr<SceneModel> IfcLoader::LoadDocument(const std::string& filepath, LoadState* state) {
    auto totalStartTime = std::chrono::high_resolution_clock::now();

    BIM_LOG("IfcLoader", "Starting load: " << filepath);
    auto setErr = [&](const std::string& msg) {
        BIM_ERR("IfcLoader", msg);
        if (state) { state->hasError.store(true); state->SetStatus(msg, 1.0f); }
    };

    if (state) { state->hasError.store(false); state->SetStatus("Verifying file...", 0.05f); }

    std::filesystem::path safePath = std::filesystem::path(reinterpret_cast<const char8_t*>(filepath.c_str()));
    
    std::error_code ec;
    auto lastWriteTime = std::filesystem::last_write_time(safePath, ec);
    uint64_t fileTimestamp = ec ? 0 : lastWriteTime.time_since_epoch().count();
    
    std::ifstream check(safePath, std::ios::binary | std::ios::ate);
    if (!check.good()) {
        setErr("File not found or inaccessible: " + filepath);
        return nullptr;
    }
    size_t fileSize = check.tellg();
    check.close();

    try {
        // --- TIMING PHASE 1: AST Parsing (Always Required) ---
        auto phaseStart = std::chrono::high_resolution_clock::now();
        if (state) state->SetStatus("Parsing IFC schema...", 0.10f);
        auto ifcDb = std::make_shared<IfcParse::IfcFile>(filepath);
        if (!ifcDb->good()) { setErr("Corrupted or invalid IFC schema."); return nullptr; }
        
        std::chrono::duration<double> parseTime = std::chrono::high_resolution_clock::now() - phaseStart;
        BIM_LOG("IfcLoader", "[Timing] AST Parsing took: " << parseTime.count() << "s");

        RenderMesh mesh;
        bool cacheLoaded = false;
        
        // --- NEW: Map Cache File to the OS Temp Directory ---
        std::filesystem::path tempDir = std::filesystem::temp_directory_path();
        std::string filename = safePath.filename().string();
        size_t pathHash = std::hash<std::string>{}(filepath); // Hash full path to avoid collisions
        std::string cacheName = std::to_string(pathHash) + "_" + filename + ".bimcache";
        std::filesystem::path cachePath = tempDir / cacheName;

        BIM_LOG("IfcLoader", "Using temp cache path: " << cachePath.string());

        // --- CACHE READ PHASE ---
        if (std::filesystem::exists(cachePath)) {
            BIM_LOG("IfcLoader", "Discovered .bimcache file. Validating...");
            std::ifstream cacheIn(cachePath, std::ios::binary);
            if (cacheIn.good()) {
                char magic[8] = {0};
                cacheIn.read(magic, sizeof(magic));
                if (std::string(magic, 7) == "BIMCACH") {
                    uint32_t version = 0;
                    cacheIn.read(reinterpret_cast<char*>(&version), sizeof(version));
                    if (version == BIMCACHE_VERSION) {
                        uint64_t cachedTimestamp = 0;
                        cacheIn.read(reinterpret_cast<char*>(&cachedTimestamp), sizeof(cachedTimestamp));
                        
                        if (cachedTimestamp == fileTimestamp) {
                            if (state) state->SetStatus("Loading from high-speed binary cache...", 0.50f);
                            auto cacheReadStart = std::chrono::high_resolution_clock::now();

                            cacheIn.read(reinterpret_cast<char*>(mesh.minBounds), sizeof(mesh.minBounds));
                            cacheIn.read(reinterpret_cast<char*>(mesh.maxBounds), sizeof(mesh.maxBounds));
                            cacheIn.read(reinterpret_cast<char*>(mesh.center), sizeof(mesh.center));

                            ReadVector(cacheIn, mesh.vertices);
                            ReadVector(cacheIn, mesh.indices);
                            ReadVector(cacheIn, mesh.bvhNodes);

                            size_t subMeshCount = 0;
                            cacheIn.read(reinterpret_cast<char*>(&subMeshCount), sizeof(subMeshCount));
                            mesh.subMeshes.resize(subMeshCount);
                            for (size_t i = 0; i < subMeshCount; ++i) {
                                auto& sub = mesh.subMeshes[i];
                                ReadString(cacheIn, sub.guid);
                                ReadString(cacheIn, sub.type);
                                cacheIn.read(reinterpret_cast<char*>(&sub.startIndex), sizeof(sub.startIndex));
                                cacheIn.read(reinterpret_cast<char*>(&sub.indexCount), sizeof(sub.indexCount));
                                cacheIn.read(reinterpret_cast<char*>(sub.center), sizeof(sub.center));
                                cacheIn.read(reinterpret_cast<char*>(&sub.isTransparent), sizeof(sub.isTransparent));
                                cacheIn.read(reinterpret_cast<char*>(&sub.textureIndex), sizeof(sub.textureIndex));
                                cacheIn.read(reinterpret_cast<char*>(&sub.bvhRootIndex), sizeof(sub.bvhRootIndex));
                                cacheIn.read(reinterpret_cast<char*>(&sub.globalStartIndex), sizeof(sub.globalStartIndex));
                                cacheIn.read(reinterpret_cast<char*>(&sub.globalTextureIndex), sizeof(sub.globalTextureIndex));
                            }

                            mesh.originalVertices = mesh.vertices;
                            cacheLoaded = true;

                            std::chrono::duration<double> cacheTime = std::chrono::high_resolution_clock::now() - cacheReadStart;
                            BIM_LOG("IfcLoader", "[Timing] Cache injection took: " << cacheTime.count() << "s");
                        } else {
                            BIM_LOG("IfcLoader", "Cache timestamp mismatch. Source file modified. Rebuilding geometry.");
                        }
                    } else {
                        BIM_LOG("IfcLoader", "Cache version mismatch. Rebuilding geometry.");
                    }
                }
            }
        }

        if (!cacheLoaded) {
            // --- TIMING PHASE 2: Kernel Init ---
            phaseStart = std::chrono::high_resolution_clock::now();
            if (state) state->SetStatus("Initialising geometry kernel...", 0.25f);
            
            ifcopenshell::geometry::Settings settings;
            settings.get<ifcopenshell::geometry::settings::UseWorldCoords>().value = false;
            settings.get<ifcopenshell::geometry::settings::MesherLinearDeflection>().value = 0.02; 
            settings.get<ifcopenshell::geometry::settings::MesherAngularDeflection>().value = 0.5;

            const unsigned int hwThreads = std::thread::hardware_concurrency();
            const int useThreads = (hwThreads > 0) ? static_cast<int>(hwThreads) : 4;
            const size_t maxInFlightTasks = useThreads * 4; 
            
            auto kernel = std::make_unique<IfcGeom::OpenCascadeKernel>(settings);
            IfcGeom::Iterator geomIter(std::move(kernel), settings, ifcDb.get(), useThreads);
            if (!geomIter.initialize()) { setErr("Geometry iterator failed to initialise."); return nullptr; }
            
            std::chrono::duration<double> initTime = std::chrono::high_resolution_clock::now() - phaseStart;
            BIM_LOG("IfcLoader", "[Timing] Kernel Init took: " << initTime.count() << "s");

            // --- TIMING PHASE 3: CSG Triangulation & BVH ---
            phaseStart = std::chrono::high_resolution_clock::now();
            if (state) state->SetStatus("Triangulating & multithreading geometry...", 0.35f);

            float globalMin[3] = { kFloatMax, kFloatMax, kFloatMax };
            float globalMax[3] = { kFloatMin, kFloatMin, kFloatMin };

            size_t estimatedVertices = (fileSize / 1024) * 15;
            mesh.vertices.reserve(estimatedVertices);
            mesh.indices.reserve(estimatedVertices * 3);
            mesh.subMeshes.reserve((fileSize / 1024) / 10);
            mesh.bvhNodes.reserve(estimatedVertices);

            ThreadPool pool(useThreads, maxInFlightTasks);
            std::deque<std::future<MeshChunk>> pendingChunks;

            auto mergeChunk = [&](MeshChunk& chunk) {
                if (chunk.vertices.empty()) return;

                const uint32_t vertexOffset = static_cast<uint32_t>(mesh.vertices.size());
                const uint32_t indexOffset = static_cast<uint32_t>(mesh.indices.size());
                const uint32_t bvhOffset = static_cast<uint32_t>(mesh.bvhNodes.size());

                mesh.vertices.insert(mesh.vertices.end(), chunk.vertices.begin(), chunk.vertices.end());
                
                mesh.indices.resize(indexOffset + chunk.indices.size());
                uint32_t* destIndices = mesh.indices.data() + indexOffset;
                const uint32_t* srcIndices = chunk.indices.data();
                for (size_t i = 0; i < chunk.indices.size(); ++i) {
                    destIndices[i] = srcIndices[i] + vertexOffset;
                }

                for (auto& sub : chunk.subMeshes) {
                    sub.startIndex += indexOffset;
                    sub.bvhRootIndex += bvhOffset;
                    mesh.subMeshes.push_back(std::move(sub));
                }

                mesh.bvhNodes.resize(bvhOffset + chunk.bvhNodes.size());
                BVHNode* destNodes = mesh.bvhNodes.data() + bvhOffset;
                const BVHNode* srcNodes = chunk.bvhNodes.data();
                for (size_t i = 0; i < chunk.bvhNodes.size(); ++i) {
                    BVHNode node = srcNodes[i];
                    if (node.triCount == 0) node.leftFirst += bvhOffset;
                    else node.leftFirst += indexOffset;
                    destNodes[i] = node;
                }

                for (int j = 0; j < 3; ++j) {
                    if (chunk.minB[j] < globalMin[j]) globalMin[j] = chunk.minB[j];
                    if (chunk.maxB[j] > globalMax[j]) globalMax[j] = chunk.maxB[j];
                }
            };

            do {
                if (state) {
                    float kernelProg = static_cast<float>(geomIter.progress()) / 100.0f;
                    state->SetStatus("Extracting Instances...", 0.35f + (kernelProg * 0.45f));
                }

                auto* elem = geomIter.get();
                if (elem) {
                    auto* triElem = dynamic_cast<IfcGeom::TriangulationElement*>(elem);
                    if (triElem) {
                        std::string guid = elem->guid();
                        std::string type = elem->type();
                        
                        std::array<double, 16> trsfMatrix = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
                        const auto& trsfData = triElem->transformation().data();
                        if (trsfData) {
                            const auto& comp = trsfData->ccomponents();
                            trsfMatrix[0]  = comp(0, 0); trsfMatrix[1]  = comp(0, 1); trsfMatrix[2]  = comp(0, 2); trsfMatrix[3]  = comp(0, 3);
                            trsfMatrix[4]  = comp(1, 0); trsfMatrix[5]  = comp(1, 1); trsfMatrix[6]  = comp(1, 2); trsfMatrix[7]  = comp(1, 3);
                            trsfMatrix[8]  = comp(2, 0); trsfMatrix[9]  = comp(2, 1); trsfMatrix[10] = comp(2, 2); trsfMatrix[11] = comp(2, 3);
                            trsfMatrix[12] = comp(3, 0); trsfMatrix[13] = comp(3, 1); trsfMatrix[14] = comp(3, 2); trsfMatrix[15] = comp(3, 3);
                        }

                        auto verts       = triElem->geometry().verts();
                        auto normals     = triElem->geometry().normals();
                        auto faces       = triElem->geometry().faces();
                        auto materials   = triElem->geometry().materials();
                        auto materialIds = triElem->geometry().material_ids();

                        pendingChunks.push_back(pool.Enqueue([
                            guid, type, trsfMatrix, verts, normals, faces, materials, materialIds
                        ]() -> MeshChunk {
                            MeshChunk chunk;
                            const int numTrisTotal = static_cast<int>(faces.size()) / 3;
                            const int numVerts = static_cast<int>(verts.size()) / 3;
                            if (numTrisTotal == 0 || numVerts == 0) return chunk;

                            chunk.vertices.resize(numVerts);
                            for (int i = 0; i < numVerts; ++i) {
                                int vOff = i * 3;
                                double lx = verts[vOff]; double ly = verts[vOff + 1]; double lz = verts[vOff + 2];
                                
                                Vertex& vertex = chunk.vertices[i];
                                vertex.position[0] = static_cast<float>(lx * trsfMatrix[0] + ly * trsfMatrix[1] + lz * trsfMatrix[2] + trsfMatrix[3]);
                                vertex.position[1] = static_cast<float>(lx * trsfMatrix[4] + ly * trsfMatrix[5] + lz * trsfMatrix[6] + trsfMatrix[7]);
                                vertex.position[2] = static_cast<float>(lx * trsfMatrix[8] + ly * trsfMatrix[9] + lz * trsfMatrix[10]+ trsfMatrix[11]);
                                
                                if (vOff + 2 < static_cast<int>(normals.size())) {
                                    double nx = normals[vOff]; double ny = normals[vOff + 1]; double nz = normals[vOff + 2];
                                    vertex.normal[0] = static_cast<float>(nx * trsfMatrix[0] + ny * trsfMatrix[1] + nz * trsfMatrix[2]);
                                    vertex.normal[1] = static_cast<float>(nx * trsfMatrix[4] + ny * trsfMatrix[5] + nz * trsfMatrix[6]);
                                    vertex.normal[2] = static_cast<float>(nx * trsfMatrix[8] + ny * trsfMatrix[9] + nz * trsfMatrix[10]);
                                    float len = std::sqrt(vertex.normal[0]*vertex.normal[0] + vertex.normal[1]*vertex.normal[1] + vertex.normal[2]*vertex.normal[2]);
                                    if (len > 0.0001f) { vertex.normal[0] /= len; vertex.normal[1] /= len; vertex.normal[2] /= len; }
                                } else { vertex.normal[2] = 1.0f; }
                                
                                for (int j = 0; j < 3; ++j) {
                                    if (vertex.position[j] < chunk.minB[j]) chunk.minB[j] = vertex.position[j];
                                    if (vertex.position[j] > chunk.maxB[j]) chunk.maxB[j] = vertex.position[j];
                                }
                            }

                            struct TempTriangle {
                                int v_idx[3];
                                int mat_id;
                            };
                            std::vector<TempTriangle> sortedTris(numTrisTotal);
                            for (int t = 0; t < numTrisTotal; ++t) {
                                sortedTris[t].v_idx[0] = faces[t * 3 + 0];
                                sortedTris[t].v_idx[1] = faces[t * 3 + 1];
                                sortedTris[t].v_idx[2] = faces[t * 3 + 2];
                                sortedTris[t].mat_id = materialIds.empty() ? -1 : materialIds[t];
                            }

                            std::stable_sort(sortedTris.begin(), sortedTris.end(), [](const TempTriangle& a, const TempTriangle& b) {
                                return a.mat_id < b.mat_id;
                            });

                            int currentMatId = -2;
                            uint32_t subStartIndex = 0;
                            float r = 0.85f, g = 0.85f, b = 0.85f;
                            bool isTrans = false;
                            float subMin[3] = { kFloatMax, kFloatMax, kFloatMax };
                            float subMax[3] = { kFloatMin, kFloatMin, kFloatMin };
                            bool hasMultipleMats = !sortedTris.empty() && (sortedTris.front().mat_id != sortedTris.back().mat_id);

                            chunk.indices.reserve(numTrisTotal * 3);

                            for (int t = 0; t < numTrisTotal; ++t) {
                                const auto& tri = sortedTris[t];

                                if (tri.mat_id != currentMatId) {
                                    if (t > 0) {
                                        const uint32_t subIndexCount = static_cast<uint32_t>(chunk.indices.size()) - subStartIndex;
                                        if (subIndexCount > 0) {
                                            RenderSubMesh sub;
                                            sub.guid = guid + (hasMultipleMats ? "_L" + std::to_string(currentMatId) : "");
                                            sub.type = type;
                                            sub.startIndex = subStartIndex;
                                            sub.indexCount = subIndexCount;
                                            sub.isTransparent = isTrans;
                                            sub.center[0] = (subMin[0] + subMax[0]) * 0.5f;
                                            sub.center[1] = (subMin[1] + subMax[1]) * 0.5f;
                                            sub.center[2] = (subMin[2] + subMax[2]) * 0.5f;
                                            chunk.subMeshes.push_back(std::move(sub));
                                        }
                                    }

                                    currentMatId = tri.mat_id;
                                    subStartIndex = static_cast<uint32_t>(chunk.indices.size());
                                    for (int j = 0; j < 3; ++j) { subMin[j] = kFloatMax; subMax[j] = kFloatMin; }
                                    
                                    r = 0.85f; g = 0.85f; b = 0.85f; isTrans = false;
                                    if (currentMatId >= 0 && currentMatId < static_cast<int>(materials.size())) {
                                        auto mat = materials[currentMatId];
                                        if (mat) {
                                            r = static_cast<float>(mat->diffuse.r());
                                            g = static_cast<float>(mat->diffuse.g());
                                            b = static_cast<float>(mat->diffuse.b());
                                            if (mat->transparency > 0.0) isTrans = true;
                                        }
                                    } else if (!materials.empty() && materials[0]) {
                                        auto mat = materials[0];
                                        r = static_cast<float>(mat->diffuse.r());
                                        g = static_cast<float>(mat->diffuse.g());
                                        b = static_cast<float>(mat->diffuse.b());
                                        if (mat->transparency > 0.0) isTrans = true;
                                    }
                                }

                                for (int v = 0; v < 3; ++v) {
                                    const int idx = tri.v_idx[v];
                                    chunk.indices.push_back(idx);
                                    chunk.vertices[idx].color[0] = r;
                                    chunk.vertices[idx].color[1] = g;
                                    chunk.vertices[idx].color[2] = b;

                                    for (int j = 0; j < 3; ++j) {
                                        if (chunk.vertices[idx].position[j] < subMin[j]) subMin[j] = chunk.vertices[idx].position[j];
                                        if (chunk.vertices[idx].position[j] > subMax[j]) subMax[j] = chunk.vertices[idx].position[j];
                                    }
                                }
                            }

                            const uint32_t subIndexCount = static_cast<uint32_t>(chunk.indices.size()) - subStartIndex;
                            if (subIndexCount > 0) {
                                RenderSubMesh sub;
                                sub.guid = guid + (hasMultipleMats ? "_L" + std::to_string(currentMatId) : "");
                                sub.type = type;
                                sub.startIndex = subStartIndex;
                                sub.indexCount = subIndexCount;
                                sub.isTransparent = isTrans;
                                sub.center[0] = (subMin[0] + subMax[0]) * 0.5f;
                                sub.center[1] = (subMin[1] + subMax[1]) * 0.5f;
                                sub.center[2] = (subMin[2] + subMax[2]) * 0.5f;
                                chunk.subMeshes.push_back(std::move(sub));
                            }

                            chunk.bvhNodes.reserve(numTrisTotal * 2);
                            struct Centroid { float center[3]; };
                            std::vector<Centroid> centroids;

                            for (auto& sub : chunk.subMeshes) {
                                uint32_t numTris = sub.indexCount / 3;
                                if (numTris == 0) continue;

                                centroids.resize(numTris);
                                for (uint32_t i = 0; i < numTris; ++i) {
                                    uint32_t i0 = chunk.indices[sub.startIndex + i * 3];
                                    uint32_t i1 = chunk.indices[sub.startIndex + i * 3 + 1];
                                    uint32_t i2 = chunk.indices[sub.startIndex + i * 3 + 2];
                                    for (int j = 0; j < 3; ++j) {
                                        centroids[i].center[j] = (chunk.vertices[i0].position[j] + chunk.vertices[i1].position[j] + chunk.vertices[i2].position[j]) / 3.0f;
                                    }
                                }

                                auto updateBounds = [&](uint32_t nodeIdx, uint32_t first, uint32_t count) {
                                    BVHNode& node = chunk.bvhNodes[nodeIdx];
                                    for (int j = 0; j < 3; ++j) { node.aabbMin[j] = 1e9f; node.aabbMax[j] = -1e9f; }
                                    for (uint32_t i = 0; i < count; ++i) {
                                        for (int v = 0; v < 3; ++v) {
                                            uint32_t vi = chunk.indices[sub.startIndex + (first + i) * 3 + v];
                                            for (int j = 0; j < 3; ++j) {
                                                node.aabbMin[j] = std::min(node.aabbMin[j], chunk.vertices[vi].position[j]);
                                                node.aabbMax[j] = std::max(node.aabbMax[j], chunk.vertices[vi].position[j]);
                                            }
                                        }
                                    }
                                };

                                auto subdivide = [&](auto& self, uint32_t nodeIdx, uint32_t first, uint32_t count) -> void {
                                    if (count <= 32) {
                                        chunk.bvhNodes[nodeIdx].leftFirst = sub.startIndex + first * 3;
                                        chunk.bvhNodes[nodeIdx].triCount = count;
                                        return;
                                    }

                                    float extent[3];
                                    for (int i = 0; i < 3; ++i) extent[i] = chunk.bvhNodes[nodeIdx].aabbMax[i] - chunk.bvhNodes[nodeIdx].aabbMin[i];
                                    int axis = 0;
                                    if (extent[1] > extent[0]) axis = 1;
                                    if (extent[2] > extent[axis]) axis = 2;

                                    float splitPos = chunk.bvhNodes[nodeIdx].aabbMin[axis] + extent[axis] * 0.5f;

                                    uint32_t i = first;
                                    uint32_t j = first + count - 1;
                                    while (i <= j) {
                                        if (centroids[i].center[axis] < splitPos) {
                                            i++;
                                        } else {
                                            std::swap(centroids[i], centroids[j]);
                                            std::swap(chunk.indices[sub.startIndex + i * 3],     chunk.indices[sub.startIndex + j * 3]);
                                            std::swap(chunk.indices[sub.startIndex + i * 3 + 1], chunk.indices[sub.startIndex + j * 3 + 1]);
                                            std::swap(chunk.indices[sub.startIndex + i * 3 + 2], chunk.indices[sub.startIndex + j * 3 + 2]);
                                            if (j == 0) break;
                                            j--;
                                        }
                                    }

                                    uint32_t leftCount = i - first;
                                    if (leftCount == 0 || leftCount == count) {
                                        leftCount = count / 2;
                                        i = first + leftCount;
                                    }

                                    uint32_t leftChildIdx = static_cast<uint32_t>(chunk.bvhNodes.size());
                                    chunk.bvhNodes.emplace_back();
                                    uint32_t rightChildIdx = static_cast<uint32_t>(chunk.bvhNodes.size());
                                    chunk.bvhNodes.emplace_back();

                                    chunk.bvhNodes[nodeIdx].leftFirst = leftChildIdx;
                                    chunk.bvhNodes[nodeIdx].triCount = 0;

                                    updateBounds(leftChildIdx, first, leftCount);
                                    updateBounds(rightChildIdx, i, count - leftCount);

                                    self(self, leftChildIdx, first, leftCount);
                                    self(self, rightChildIdx, i, count - leftCount);
                                };

                                sub.bvhRootIndex = static_cast<uint32_t>(chunk.bvhNodes.size());
                                chunk.bvhNodes.emplace_back();
                                updateBounds(sub.bvhRootIndex, 0, numTris);
                                subdivide(subdivide, sub.bvhRootIndex, 0, numTris);
                            }

                            return chunk;
                        }));
                    }
                }

                while (!pendingChunks.empty()) {
                    if (pendingChunks.size() >= maxInFlightTasks) {
                        MeshChunk chunk = pendingChunks.front().get();
                        mergeChunk(chunk);
                        pendingChunks.pop_front();
                    } else if (pendingChunks.front().wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                        MeshChunk chunk = pendingChunks.front().get();
                        mergeChunk(chunk);
                        pendingChunks.pop_front();
                    } else {
                        break;
                    }
                }

            } while (geomIter.next());

            std::chrono::duration<double> csgTime = std::chrono::high_resolution_clock::now() - phaseStart;
            BIM_LOG("IfcLoader", "[Timing] CSG Triangulation & BVH took: " << csgTime.count() << "s");

            if (state) state->SetStatus("Finalising VBO Buffers...", 0.85f);
            
            while (!pendingChunks.empty()) {
                MeshChunk chunk = pendingChunks.front().get();
                mergeChunk(chunk);
                pendingChunks.pop_front();
            }

            for (int j = 0; j < 3; ++j) {
                mesh.minBounds[j] = globalMin[j];
                mesh.maxBounds[j] = globalMax[j];
                mesh.center[j] = (globalMin[j] + globalMax[j]) * 0.5f;
            }

            mesh.vertices.shrink_to_fit();
            mesh.indices.shrink_to_fit();
            mesh.subMeshes.shrink_to_fit();
            mesh.bvhNodes.shrink_to_fit();
            mesh.originalVertices = mesh.vertices;

            BIM_LOG("IfcLoader", "Geometry loaded — " << mesh.vertices.size() << " verts, " << mesh.subMeshes.size() << " submeshes.");

            // --- CACHE WRITE PHASE ---
            if (state) state->SetStatus("Writing binary cache to disk...", 0.90f);
            auto cacheWriteStart = std::chrono::high_resolution_clock::now();
            std::ofstream cacheOut(cachePath, std::ios::binary);
            if (cacheOut.good()) {
                cacheOut.write(BIMCACHE_MAGIC, sizeof(BIMCACHE_MAGIC));
                cacheOut.write(reinterpret_cast<const char*>(&BIMCACHE_VERSION), sizeof(BIMCACHE_VERSION));
                cacheOut.write(reinterpret_cast<const char*>(&fileTimestamp), sizeof(fileTimestamp));

                cacheOut.write(reinterpret_cast<const char*>(mesh.minBounds), sizeof(mesh.minBounds));
                cacheOut.write(reinterpret_cast<const char*>(mesh.maxBounds), sizeof(mesh.maxBounds));
                cacheOut.write(reinterpret_cast<const char*>(mesh.center), sizeof(mesh.center));

                WriteVector(cacheOut, mesh.vertices);
                WriteVector(cacheOut, mesh.indices);
                WriteVector(cacheOut, mesh.bvhNodes);

                size_t subMeshCount = mesh.subMeshes.size();
                cacheOut.write(reinterpret_cast<const char*>(&subMeshCount), sizeof(subMeshCount));
                for (const auto& sub : mesh.subMeshes) {
                    WriteString(cacheOut, sub.guid);
                    WriteString(cacheOut, sub.type);
                    cacheOut.write(reinterpret_cast<const char*>(&sub.startIndex), sizeof(sub.startIndex));
                    cacheOut.write(reinterpret_cast<const char*>(&sub.indexCount), sizeof(sub.indexCount));
                    cacheOut.write(reinterpret_cast<const char*>(sub.center), sizeof(sub.center));
                    cacheOut.write(reinterpret_cast<const char*>(&sub.isTransparent), sizeof(sub.isTransparent));
                    cacheOut.write(reinterpret_cast<const char*>(&sub.textureIndex), sizeof(sub.textureIndex));
                    cacheOut.write(reinterpret_cast<const char*>(&sub.bvhRootIndex), sizeof(sub.bvhRootIndex));
                    cacheOut.write(reinterpret_cast<const char*>(&sub.globalStartIndex), sizeof(sub.globalStartIndex));
                    cacheOut.write(reinterpret_cast<const char*>(&sub.globalTextureIndex), sizeof(sub.globalTextureIndex));
                }
                
                std::chrono::duration<double> cacheTime = std::chrono::high_resolution_clock::now() - cacheWriteStart;
                BIM_LOG("IfcLoader", "[Timing] Cache generation took: " << cacheTime.count() << "s");
            } else {
                BIM_LOG("IfcLoader", "Warning: Failed to open " << cachePath.string() << " for cache writing.");
            }
        } // End of if (!cacheLoaded)

        auto doc = std::make_shared<SceneModel>(ifcDb, std::move(mesh), filepath);

        if (state) state->SetStatus("Resolving spatial hierarchy...", 0.95f);

        std::unordered_map<std::string, std::string> childToParent;
        std::unordered_map<std::string, std::vector<std::string>> parentToChildren;

        auto aggregates = ifcDb->instances_by_type("IfcRelAggregates");
        if (aggregates) {
            for (auto* inst : *aggregates) {
                if (auto rel4 = inst->as<Ifc4::IfcRelAggregates>()) {
                    auto parent = rel4->RelatingObject();
                    auto children = rel4->RelatedObjects();
                    if (!parent || !children) continue;
                    std::string pGuid = parent->GlobalId();
                    for (auto* child : *children) {
                        std::string cGuid = child->GlobalId();
                        childToParent[cGuid] = pGuid;
                        parentToChildren[pGuid].push_back(cGuid);
                    }
                } else if (auto rel2 = inst->as<Ifc2x3::IfcRelAggregates>()) {
                    auto parent = rel2->RelatingObject();
                    auto children = rel2->RelatedObjects();
                    if (!parent || !children) continue;
                    std::string pGuid = parent->GlobalId();
                    for (auto* child : *children) {
                        std::string cGuid = child->GlobalId();
                        childToParent[cGuid] = pGuid;
                        parentToChildren[pGuid].push_back(cGuid);
                    }
                }
            }
        }

        auto contained = ifcDb->instances_by_type("IfcRelContainedInSpatialStructure");
        if (contained) {
            for (auto* inst : *contained) {
                if (auto rel4 = inst->as<Ifc4::IfcRelContainedInSpatialStructure>()) {
                    auto parent = rel4->RelatingStructure();
                    auto children = rel4->RelatedElements();
                    if (!parent || !children) continue;
                    std::string pGuid = parent->GlobalId();
                    for (auto* child : *children) {
                        std::string cGuid = child->GlobalId();
                        childToParent[cGuid] = pGuid;
                        parentToChildren[pGuid].push_back(cGuid);
                    }
                } else if (auto rel2 = inst->as<Ifc2x3::IfcRelContainedInSpatialStructure>()) {
                    auto parent = rel2->RelatingStructure();
                    auto children = rel2->RelatedElements();
                    if (!parent || !children) continue;
                    std::string pGuid = parent->GlobalId();
                    for (auto* child : *children) {
                        std::string cGuid = child->GlobalId();
                        childToParent[cGuid] = pGuid;
                        parentToChildren[pGuid].push_back(cGuid);
                    }
                }
            }
        }

        doc->SetHierarchy(childToParent, parentToChildren);
        BIM_LOG("IfcLoader", "Hierarchy securely resolved: " << parentToChildren.size() << " containers mapped.");

        std::chrono::duration<double> elapsed = std::chrono::high_resolution_clock::now() - totalStartTime;
        BIM_LOG("IfcLoader", "Successfully loaded '" << filepath << "' in " << elapsed.count() << " seconds.");

        if (state) state->SetStatus("Ready.", 1.0f);
        return doc;

    } catch (const std::exception& e) {
        setErr(std::string("Exception: ") + e.what());
    } catch (...) {
        setErr("Unknown fatal exception during parse.");
    }
    return nullptr;
}

} // namespace BimCore