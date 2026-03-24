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

#include <ifcparse/IfcFile.h>
#include <ifcgeom/Iterator.h>
#include <ifcgeom/kernels/opencascade/OpenCascadeKernel.h>
#include <ifcparse/Ifc2x3.h>
#include <ifcparse/Ifc4.h>

namespace BimCore {

    std::shared_ptr<SceneModel> IfcLoader::LoadDocument(const std::string& filepath, LoadState* state) {
        BIM_LOG("IfcLoader", "Starting load: " << filepath);
        auto setErr = [&](const std::string& msg) {
            BIM_ERR("IfcLoader", msg);
            if (state) { state->hasError.store(true); state->SetStatus(msg, 1.0f); }
        };

        if (state) { state->hasError.store(false); state->SetStatus("Verifying file...", 0.05f); }

        std::ifstream check(filepath, std::ios::binary | std::ios::ate);
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
            settings.get<ifcopenshell::geometry::settings::UseWorldCoords>().value = true;
            settings.get<ifcopenshell::geometry::settings::MesherLinearDeflection>().value = 0.005;

            const unsigned int hwThreads = std::thread::hardware_concurrency();
            const int useThreads = (hwThreads > 0) ? static_cast<int>(hwThreads) : 2;
            BIM_LOG("IfcLoader", "Geometry kernel threads: " << useThreads);

            auto kernel = std::make_unique<IfcGeom::OpenCascadeKernel>(settings);
            IfcGeom::Iterator geomIter(std::move(kernel), settings, ifcDb.get(), useThreads);
            if (!geomIter.initialize()) { setErr("Geometry iterator failed to initialise."); return nullptr; }

            if (state) state->SetStatus("Triangulating geometry...", 0.35f);

            RenderMesh mesh;
            float minB[3] = { kFloatMax, kFloatMax, kFloatMax };
            float maxB[3] = { kFloatMin, kFloatMin, kFloatMin };

            size_t estimatedVertices = (fileSize / 1024) * 15;
            size_t estimatedIndices = estimatedVertices * 3;
            size_t estimatedSubMeshes = (fileSize / 1024) / 10;

            mesh.vertices.reserve(estimatedVertices);
            mesh.indices.reserve(estimatedIndices);
            mesh.subMeshes.reserve(estimatedSubMeshes);

            BIM_LOG("IfcLoader", "Reserved VRAM capacity for ~" << estimatedVertices << " vertices based on file size.");

            do {
                if (state) {
                    float kernelProg = static_cast<float>(geomIter.progress()) / 100.0f;
                    state->SetStatus("Triangulating geometry...", 0.35f + (kernelProg * 0.55f));
                }

                auto* elem = geomIter.get();
                if (!elem) continue;
                auto* triElem = dynamic_cast<IfcGeom::TriangulationElement*>(elem);
                if (!triElem) continue;

                const std::string guid    = elem->guid();
                const std::string type    = elem->type();
                const auto& geom          = triElem->geometry();
                const auto& verts         = geom.verts();
                const auto& normals       = geom.normals();
                const auto& faces         = geom.faces();
                const auto& materials     = geom.materials();
                const auto& materialIds   = geom.material_ids();
                const int numTris = static_cast<int>(faces.size()) / 3;

                std::unordered_set<int> usedMatSet;
                if (materialIds.empty()) usedMatSet.insert(-1);
                else for (int mid : materialIds) usedMatSet.insert(mid);

                for (int matIdx : usedMatSet) {
                    const uint32_t subStartIndex = static_cast<uint32_t>(mesh.indices.size());
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

                                vertex.position[0] = static_cast<float>(verts[vOff]);
                                vertex.position[1] = static_cast<float>(verts[vOff + 1]);
                                vertex.position[2] = static_cast<float>(verts[vOff + 2]);

                                for (int j = 0; j < 3; ++j) {
                                    if (vertex.position[j] < subMin[j]) subMin[j] = vertex.position[j];
                                    if (vertex.position[j] > subMax[j]) subMax[j] = vertex.position[j];
                                    if (vertex.position[j] < minB[j])   minB[j]   = vertex.position[j];
                                    if (vertex.position[j] > maxB[j])   maxB[j]   = vertex.position[j];
                                }
                                if (vOff + 2 < static_cast<int>(normals.size())) {
                                    vertex.normal[0] = static_cast<float>(normals[vOff]);
                                    vertex.normal[1] = static_cast<float>(normals[vOff + 1]);
                                    vertex.normal[2] = static_cast<float>(normals[vOff + 2]);
                                } else { vertex.normal[2] = 1.0f; }
                                vertex.color[0] = r; vertex.color[1] = g; vertex.color[2] = b;
                                it->second = static_cast<uint32_t>(mesh.vertices.size());
                                mesh.vertices.push_back(vertex);
                            }
                            mesh.indices.push_back(it->second);
                        }
                    }
                    const uint32_t subIndexCount = static_cast<uint32_t>(mesh.indices.size()) - subStartIndex;
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
                    mesh.subMeshes.push_back(std::move(sub));
                }
            } while (geomIter.next());

            for (int j = 0; j < 3; ++j) {
                mesh.minBounds[j] = minB[j];
                mesh.maxBounds[j] = maxB[j];
                mesh.center[j] = (minB[j] + maxB[j]) * 0.5f;
            }

            mesh.vertices.shrink_to_fit();
            mesh.indices.shrink_to_fit();
            mesh.subMeshes.shrink_to_fit();
            mesh.originalVertices = mesh.vertices;

            BIM_LOG("IfcLoader", "Geometry loaded — " << mesh.vertices.size() << " verts, " << mesh.subMeshes.size() << " submeshes.");

            auto doc = std::make_shared<SceneModel>(ifcDb, std::move(mesh), filepath);

            // =================================================================
            // FIXED: AST-Based Spatial Hierarchy Extraction
            // =================================================================
            if (state) state->SetStatus("Resolving spatial hierarchy...", 0.95f);

            std::unordered_map<std::string, std::string> childToParent;
            std::unordered_map<std::string, std::vector<std::string>> parentToChildren;

            // Pass 1: Extract Spatial Aggregation (Project -> Site -> Building -> Storey)
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

            // Pass 2: Extract Spatial Containment (Storey -> Walls, Doors, Elements)
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

}