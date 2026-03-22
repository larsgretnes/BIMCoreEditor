// =============================================================================
// BimCore/scene/IfcLoader.cpp
// =============================================================================
#include "IfcLoader.h"
#include "Core.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include <ifcparse/IfcFile.h>
#include <ifcgeom/Iterator.h>
#include <ifcgeom/kernels/opencascade/OpenCascadeKernel.h>

namespace BimCore {

    std::shared_ptr<BimDocument> IfcLoader::LoadDocument(const std::string& filepath, LoadState* state) {
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

                                // --- FIXED: Strict 1:1 Mapping for Z-Up ---
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
                                    // --- FIXED: Strict 1:1 Mapping for Normals ---
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

            auto doc = std::make_shared<BimDocument>(ifcDb, std::move(mesh), filepath);

            // --- 5. DIRECT TEXT-BASED HIERARCHY PARSER ---
            if (state) state->SetStatus("Resolving spatial hierarchy...", 0.95f);

            std::unordered_map<int, std::string> stepIdToGuid;
            std::vector<std::pair<int, std::vector<int>>> rawAggregates;

            std::ifstream infile(filepath);
            std::string line, buffer;
            while (std::getline(infile, line)) {
                line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
                line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
                buffer += line;
                if (buffer.empty()) continue;

                if (buffer.back() == ';') {
                    if (buffer[0] == '#') {
                        size_t eqPos = buffer.find('=');
                        if (eqPos != std::string::npos) {
                            int id = std::stoi(buffer.substr(1, eqPos - 1));

                            size_t q1 = buffer.find('\'', eqPos);
                            if (q1 != std::string::npos) {
                                size_t q2 = buffer.find('\'', q1 + 1);
                                if (q2 != std::string::npos && (q2 - q1) == 23) {
                                    stepIdToGuid[id] = buffer.substr(q1 + 1, 22);
                                }
                            }

                            if (buffer.find("IFCRELAGGREGATES") != std::string::npos) {
                                size_t listEnd = buffer.find_last_of(')');
                                if (listEnd != std::string::npos) listEnd = buffer.find_last_of(')', listEnd - 1);
                                size_t listStart = std::string::npos;
                                if (listEnd != std::string::npos) listStart = buffer.find_last_of('(', listEnd);

                                if (listStart != std::string::npos && listEnd != std::string::npos && listStart < listEnd) {
                                    size_t relatingEnd = buffer.find_last_of(',', listStart);
                                    if (relatingEnd != std::string::npos) {
                                        size_t relatingStart = buffer.find_last_of(",(", relatingEnd - 1);
                                        if (relatingStart != std::string::npos) {
                                            std::string relatingStr = buffer.substr(relatingStart + 1, relatingEnd - relatingStart - 1);
                                            size_t hashPos = relatingStr.find('#');
                                            if (hashPos != std::string::npos) {
                                                int parentId = std::stoi(relatingStr.substr(hashPos + 1));
                                                std::vector<int> childIds;
                                                std::string childrenStr = buffer.substr(listStart + 1, listEnd - listStart - 1);
                                                size_t pos = 0;
                                                while ((pos = childrenStr.find('#', pos)) != std::string::npos) {
                                                    int childId = std::stoi(childrenStr.substr(pos + 1));
                                                    childIds.push_back(childId);
                                                    pos++;
                                                }
                                                if (!childIds.empty()) {
                                                    rawAggregates.push_back({parentId, childIds});
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    buffer.clear();
                }
            }

            std::unordered_map<std::string, std::string> childToParent;
            std::unordered_map<std::string, std::vector<std::string>> parentToChildren;

            for (const auto& agg : rawAggregates) {
                if (stepIdToGuid.count(agg.first) > 0) {
                    std::string pGuid = stepIdToGuid[agg.first];
                    for (int cId : agg.second) {
                        if (stepIdToGuid.count(cId) > 0) {
                            std::string cGuid = stepIdToGuid[cId];
                            childToParent[cGuid] = pGuid;
                            parentToChildren[pGuid].push_back(cGuid);
                        }
                    }
                }
            }

            doc->SetHierarchy(childToParent, parentToChildren);
            BIM_LOG("IfcLoader", "Hierarchy resolved via direct text parsing: " << parentToChildren.size() << " assemblies found.");

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
