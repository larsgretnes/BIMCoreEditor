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

#include <ifcparse/IfcFile.h>
#include <ifcgeom/Iterator.h>
#include <ifcgeom/kernels/opencascade/OpenCascadeKernel.h>
#include <ifcparse/Ifc2x3.h>
#include <ifcparse/Ifc4.h>

namespace BimCore {

namespace {
    // =========================================================================
    // Enterprise Thread Pool with Backpressure Controls
    // =========================================================================
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
        float minB[3] = { kFloatMax, kFloatMax, kFloatMax };
        float maxB[3] = { kFloatMin, kFloatMin, kFloatMin };
    };
}

std::shared_ptr<SceneModel> IfcLoader::LoadDocument(const std::string& filepath, LoadState* state) {
    BIM_LOG("IfcLoader", "Starting load: " << filepath);
    auto setErr = [&](const std::string& msg) {
        BIM_ERR("IfcLoader", msg);
        if (state) { state->hasError.store(true); state->SetStatus(msg, 1.0f); }
    };

    if (state) { state->hasError.store(false); state->SetStatus("Verifying file...", 0.05f); }

    // Cast the UTF-8 std::string to char8_t to force safe C++20 cross-platform path resolution
    std::filesystem::path safePath = std::filesystem::path(reinterpret_cast<const char8_t*>(filepath.c_str()));
    
    std::ifstream check(safePath, std::ios::binary | std::ios::ate);
    if (!check.good()) {
        setErr("File not found or inaccessible: " + filepath);
        return nullptr;
    }
    size_t fileSize = check.tellg();
    check.close();

    try {
        if (state) state->SetStatus("Parsing IFC schema...", 0.10f);
        auto ifcDb = std::make_shared<IfcParse::IfcFile>(filepath);
        if (!ifcDb->good()) { setErr("Corrupted or invalid IFC schema."); return nullptr; }

        if (state) state->SetStatus("Initialising geometry kernel...", 0.25f);
        
        ifcopenshell::geometry::Settings settings;
        settings.get<ifcopenshell::geometry::settings::UseWorldCoords>().value = false;
        settings.get<ifcopenshell::geometry::settings::MesherLinearDeflection>().value = 0.005;

        const unsigned int hwThreads = std::thread::hardware_concurrency();
        const int useThreads = (hwThreads > 0) ? static_cast<int>(hwThreads) : 4;
        const size_t maxInFlightTasks = useThreads * 4; 
        
        BIM_LOG("IfcLoader", "Hardware threads allocated: " << useThreads << " (Max queue: " << maxInFlightTasks << ")");

        auto kernel = std::make_unique<IfcGeom::OpenCascadeKernel>(settings);
        IfcGeom::Iterator geomIter(std::move(kernel), settings, ifcDb.get(), useThreads);
        if (!geomIter.initialize()) { setErr("Geometry iterator failed to initialise."); return nullptr; }

        if (state) state->SetStatus("Triangulating & multithreading geometry...", 0.35f);

        RenderMesh mesh;
        float globalMin[3] = { kFloatMax, kFloatMax, kFloatMax };
        float globalMax[3] = { kFloatMin, kFloatMin, kFloatMin };

        size_t estimatedVertices = (fileSize / 1024) * 15;
        mesh.vertices.reserve(estimatedVertices);
        mesh.indices.reserve(estimatedVertices * 3);
        mesh.subMeshes.reserve((fileSize / 1024) / 10);

        ThreadPool pool(useThreads, maxInFlightTasks);
        std::deque<std::future<MeshChunk>> pendingChunks;

        auto mergeChunk = [&](MeshChunk& chunk) {
            if (chunk.vertices.empty()) return;

            const uint32_t vertexOffset = static_cast<uint32_t>(mesh.vertices.size());
            const uint32_t indexOffset = static_cast<uint32_t>(mesh.indices.size());

            mesh.vertices.insert(mesh.vertices.end(), chunk.vertices.begin(), chunk.vertices.end());
            
            for (uint32_t idx : chunk.indices) {
                mesh.indices.push_back(idx + vertexOffset);
            }

            for (auto& sub : chunk.subMeshes) {
                sub.startIndex += indexOffset;
                mesh.subMeshes.push_back(std::move(sub));
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
                        // FIXED: Explicitly use Eigen's 0-indexed (row, col) access 
                        // to guarantee row-major mapping for our vertex transforms.
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
                        const int numTris = static_cast<int>(faces.size()) / 3;
                        if (numTris == 0) return chunk;

                        std::unordered_set<int> usedMatSet;
                        if (materialIds.empty()) usedMatSet.insert(-1);
                        else for (int mid : materialIds) usedMatSet.insert(mid);

                        for (int matIdx : usedMatSet) {
                            const uint32_t subStartIndex = static_cast<uint32_t>(chunk.indices.size());
                            float r = 0.85f, g = 0.85f, b = 0.85f;
                            bool  isTrans = false;

                            if (matIdx >= 0 && matIdx < static_cast<int>(materials.size())) {
                                auto mat = materials[matIdx];
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

                            std::unordered_map<int, uint32_t> localVertMap;
                            localVertMap.reserve(static_cast<size_t>(numTris) * 3);
                            float subMin[3] = { kFloatMax, kFloatMax, kFloatMax };
                            float subMax[3] = { kFloatMin, kFloatMin, kFloatMin };

                            for (int t = 0; t < numTris; ++t) {
                                const int triMat = materialIds.empty() ? -1 : materialIds[t];
                                if (triMat != matIdx) continue;
                                
                                for (int v = 0; v < 3; ++v) {
                                    const int origIdx = faces[t * 3 + v];
                                    const int vOff = origIdx * 3;
                                    auto [it, inserted] = localVertMap.emplace(origIdx, 0u);
                                    
                                    if (inserted) {
                                        Vertex vertex = {};
                                        double lx = verts[vOff];
                                        double ly = verts[vOff + 1];
                                        double lz = verts[vOff + 2];

                                        vertex.position[0] = static_cast<float>(lx * trsfMatrix[0] + ly * trsfMatrix[1] + lz * trsfMatrix[2] + trsfMatrix[3]);
                                        vertex.position[1] = static_cast<float>(lx * trsfMatrix[4] + ly * trsfMatrix[5] + lz * trsfMatrix[6] + trsfMatrix[7]);
                                        vertex.position[2] = static_cast<float>(lx * trsfMatrix[8] + ly * trsfMatrix[9] + lz * trsfMatrix[10]+ trsfMatrix[11]);

                                        for (int j = 0; j < 3; ++j) {
                                            if (vertex.position[j] < subMin[j]) subMin[j] = vertex.position[j];
                                            if (vertex.position[j] > subMax[j]) subMax[j] = vertex.position[j];
                                            if (vertex.position[j] < chunk.minB[j]) chunk.minB[j] = vertex.position[j];
                                            if (vertex.position[j] > chunk.maxB[j]) chunk.maxB[j] = vertex.position[j];
                                        }

                                        if (vOff + 2 < static_cast<int>(normals.size())) {
                                            double nx = normals[vOff];
                                            double ny = normals[vOff + 1];
                                            double nz = normals[vOff + 2];
                                            vertex.normal[0] = static_cast<float>(nx * trsfMatrix[0] + ny * trsfMatrix[1] + nz * trsfMatrix[2]);
                                            vertex.normal[1] = static_cast<float>(nx * trsfMatrix[4] + ny * trsfMatrix[5] + nz * trsfMatrix[6]);
                                            vertex.normal[2] = static_cast<float>(nx * trsfMatrix[8] + ny * trsfMatrix[9] + nz * trsfMatrix[10]);
                                            
                                            float len = std::sqrt(vertex.normal[0]*vertex.normal[0] + vertex.normal[1]*vertex.normal[1] + vertex.normal[2]*vertex.normal[2]);
                                            if (len > 0.0001f) {
                                                vertex.normal[0] /= len; vertex.normal[1] /= len; vertex.normal[2] /= len;
                                            }
                                        } else { vertex.normal[2] = 1.0f; }

                                        vertex.color[0] = r; vertex.color[1] = g; vertex.color[2] = b;
                                        it->second = static_cast<uint32_t>(chunk.vertices.size());
                                        chunk.vertices.push_back(vertex);
                                    }
                                    chunk.indices.push_back(it->second);
                                }
                            }

                            const uint32_t subIndexCount = static_cast<uint32_t>(chunk.indices.size()) - subStartIndex;
                            if (subIndexCount == 0) continue;
                            RenderSubMesh sub;
                            sub.guid = guid + (usedMatSet.size() > 1 ? "_L" + std::to_string(matIdx) : "");
                            sub.type = type;
                            sub.startIndex = subStartIndex;
                            sub.indexCount = subIndexCount;
                            sub.isTransparent = isTrans;
                            sub.center[0] = (subMin[0] + subMax[0]) * 0.5f;
                            sub.center[1] = (subMin[1] + subMax[1]) * 0.5f;
                            sub.center[2] = (subMin[2] + subMax[2]) * 0.5f;
                            chunk.subMeshes.push_back(std::move(sub));
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
        mesh.originalVertices = mesh.vertices;

        BIM_LOG("IfcLoader", "Geometry loaded — " << mesh.vertices.size() << " verts, " << mesh.subMeshes.size() << " submeshes.");

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