// =============================================================================
// BimCore/scene/SceneContext.cpp
// =============================================================================
#include "SceneContext.h"
#include "core/Core.h"
#include <algorithm>
#include <execution>

namespace BimCore {

    void SceneContext::AddDocument(std::shared_ptr<SceneModel> doc) {
        m_documents.push_back(doc);
    }

    SelectionBounds SceneContext::ComputeSelectionBounds(const std::vector<SelectedObject>& objects) const {
        SelectionBounds b;
        for (const auto& obj : objects) {
            for (uint32_t i = 0; i < obj.indexCount; ++i) {
                const uint32_t vi = m_masterIndices[obj.startIndex + i];
                const float* p  = m_masterVertices[vi].position;
                for (int j = 0; j < 3; ++j) {
                    if (p[j] < b.min[j]) b.min[j] = p[j];
                    if (p[j] > b.max[j]) b.max[j] = p[j];
                }
                b.valid = true;
            }
        }
        return b;
    }

    void SceneContext::RebuildMasterMesh(GraphicsContext* graphics, SelectionState& uiState) {
        m_masterVertices.clear();
        m_masterIndices.clear();
        std::vector<TextureData> masterTextures;

        size_t totalVertices = 0;
        size_t totalIndices = 0;
        size_t totalTextures = 0;
        for (const auto& doc : m_documents) {
            totalVertices += doc->GetGeometry().vertices.size();
            totalIndices += doc->GetGeometry().indices.size();
            totalTextures += doc->GetGeometry().textures.size();
        }

        m_masterVertices.reserve(totalVertices);
        m_masterIndices.reserve(totalIndices);
        masterTextures.reserve(totalTextures);

        uint32_t vOffset = 0;
        uint32_t iOffset = 0;
        int tOffset = 0;

        float minB[3] = { kFloatMax, kFloatMax, kFloatMax };
        float maxB[3] = { kFloatMin, kFloatMin, kFloatMin };
        bool hasBounds = false;

        for (auto& doc : m_documents) {
            auto& geom = doc->GetGeometry();
            
            m_masterVertices.insert(m_masterVertices.end(), geom.vertices.begin(), geom.vertices.end());
            
            size_t currentIdxStart = m_masterIndices.size();
            m_masterIndices.resize(currentIdxStart + geom.indices.size());
            std::transform(std::execution::par_unseq, 
                           geom.indices.begin(), geom.indices.end(), 
                           m_masterIndices.begin() + currentIdxStart, 
                           [vOffset](uint32_t idx) { return idx + vOffset; });

            masterTextures.insert(masterTextures.end(), geom.textures.begin(), geom.textures.end());

            for (auto& sub : geom.subMeshes) {
                sub.globalStartIndex = sub.startIndex + iOffset;
                sub.globalTextureIndex = sub.textureIndex >= 0 ? sub.textureIndex + tOffset : -1;
            }

            for (int j=0; j<3; ++j) {
                if (geom.minBounds[j] < minB[j]) minB[j] = geom.minBounds[j];
                if (geom.maxBounds[j] > maxB[j]) maxB[j] = geom.maxBounds[j];
            }
            hasBounds = true;

            vOffset += static_cast<uint32_t>(geom.vertices.size());
            iOffset += static_cast<uint32_t>(geom.indices.size());
            tOffset += static_cast<int>(geom.textures.size());
        }

        graphics->UploadMesh(m_masterVertices, m_masterIndices);
        graphics->UploadTextures(masterTextures);

        if (hasBounds) {
            uiState.clipXMin = minB[0] - 0.1f; uiState.clipXMax = maxB[0] + 0.1f;
            uiState.clipYMin = minB[1] - 0.1f; uiState.clipYMax = maxB[1] + 0.1f;
            uiState.clipZMin = minB[2] - 0.1f; uiState.clipZMax = maxB[2] + 0.1f;
            for (int j=0; j<3; ++j) {
                minBounds[j] = minB[j];
                maxBounds[j] = maxB[j];
                uiState.sceneMinBounds[j] = minB[j];
                uiState.sceneMaxBounds[j] = maxB[j];
            }
        }

        uiState.updateGeometry = false; 
        triggerRebuild = false;
        triggerBatchRebuild = true;
    }

    void SceneContext::RebuildRenderBatches(GraphicsContext* graphics, SelectionState& uiState) {
        m_cachedSolidBatches.clear();
        m_cachedTransBatches.clear();

        for (auto& doc : m_documents) {
            if (doc->IsHidden()) continue;

            for (const auto& sub : doc->GetGeometry().subMeshes) {
                if (uiState.hiddenObjects.count(sub.guid)) continue;
                if (!uiState.showOpeningsAndSpaces && (sub.type == "IfcOpeningElement" || sub.type == "IfcSpace")) continue;

                auto& targetMap = sub.isTransparent ? m_cachedTransBatches : m_cachedSolidBatches;
                auto& targetVec = targetMap[sub.globalTextureIndex];
                
                targetVec.reserve(targetVec.size() + sub.indexCount);
                for (uint32_t i = 0; i < sub.indexCount; ++i) {
                    targetVec.push_back(m_masterIndices[sub.globalStartIndex + i]);
                }
            }
        }
        
        graphics->UpdateActiveBatches(m_cachedSolidBatches, m_cachedTransBatches);
        triggerBatchRebuild = false;
    }

    void SceneContext::UpdateGeometryOffsets(GraphicsContext* graphics, SelectionState& uiState, float explodeFactor) {
        if (!uiState.updateGeometry) return;

        float oldMin[3] = { kFloatMax, kFloatMax, kFloatMax };
        float oldMax[3] = { kFloatMin, kFloatMin, kFloatMin };
        
        if (!m_masterVertices.empty()) {
            for (const auto& v : m_masterVertices) {
                for (int j=0; j<3; ++j) {
                    if (v.position[j] < oldMin[j]) oldMin[j] = v.position[j];
                    if (v.position[j] > oldMax[j]) oldMax[j] = v.position[j];
                }
            }
        } else {
            for (auto& doc : m_documents) {
                auto& geom = doc->GetGeometry();
                for (int j=0; j<3; ++j) {
                    if (geom.minBounds[j] < oldMin[j]) oldMin[j] = geom.minBounds[j];
                    if (geom.maxBounds[j] > oldMax[j]) oldMax[j] = geom.maxBounds[j];
                }
            }
        }

        for (int j=0; j<3; ++j) {
            oldMin[j] -= 0.1f; oldMax[j] += 0.1f;
            if (oldMax[j] - oldMin[j] < 0.0001f) { oldMin[j] -= 0.1f; oldMax[j] += 0.1f; }
        }

        float pctXMin = std::clamp((uiState.clipXMin - oldMin[0]) / (oldMax[0] - oldMin[0]), 0.0f, 1.0f);
        float pctXMax = std::clamp((uiState.clipXMax - oldMin[0]) / (oldMax[0] - oldMin[0]), 0.0f, 1.0f);
        float pctYMin = std::clamp((uiState.clipYMin - oldMin[1]) / (oldMax[1] - oldMin[1]), 0.0f, 1.0f);
        float pctYMax = std::clamp((uiState.clipYMax - oldMin[1]) / (oldMax[1] - oldMin[1]), 0.0f, 1.0f);
        float pctZMin = std::clamp((uiState.clipZMin - oldMin[2]) / (oldMax[2] - oldMin[2]), 0.0f, 1.0f);
        float pctZMax = std::clamp((uiState.clipZMax - oldMin[2]) / (oldMax[2] - oldMin[2]), 0.0f, 1.0f);

        float origMin[3] = { kFloatMax, kFloatMax, kFloatMax };
        float origMax[3] = { kFloatMin, kFloatMin, kFloatMin };
        for (auto& doc : m_documents) {
            auto& geom = doc->GetGeometry();
            for (int j=0; j<3; ++j) {
                if (geom.minBounds[j] < origMin[j]) origMin[j] = geom.minBounds[j];
                if (geom.maxBounds[j] > origMax[j]) origMax[j] = geom.maxBounds[j];
            }
        }
        glm::vec3 globalCenter((origMin[0]+origMax[0])*0.5f, (origMin[1]+origMax[1])*0.5f, (origMin[2]+origMax[2])*0.5f);

        size_t totalVertices = 0;
        for (const auto& doc : m_documents) totalVertices += doc->GetGeometry().vertices.size();
        m_masterVertices.clear();
        m_masterVertices.reserve(totalVertices);
        
        float newMin[3] = { kFloatMax, kFloatMax, kFloatMax };
        float newMax[3] = { kFloatMin, kFloatMin, kFloatMin };

        for (auto& doc : m_documents) {
            auto& geom = doc->GetGeometry();
            geom.vertices = geom.originalVertices;

            std::vector<bool> shifted(geom.vertices.size(), false);

            for (const auto& sub : geom.subMeshes) {
                glm::vec3 subCenter(sub.center[0], sub.center[1], sub.center[2]);
                glm::vec3 dir = subCenter - globalCenter;
                glm::vec3 explodeOffset = dir * explodeFactor;

                glm::mat4 objMat = doc->GetObjectTransform(sub.guid);
                bool hasTransform = (objMat != glm::mat4(1.0f));
                glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(objMat)));

                if (explodeFactor > 0.01f || hasTransform) {
                    for (uint32_t i = 0; i < sub.indexCount; ++i) {
                        uint32_t vIdx = geom.indices[sub.startIndex + i];
                        if (!shifted[vIdx]) {
                            if (hasTransform) {
                                glm::vec4 p = objMat * glm::vec4(geom.vertices[vIdx].position[0], geom.vertices[vIdx].position[1], geom.vertices[vIdx].position[2], 1.0f);
                                geom.vertices[vIdx].position[0] = p.x; geom.vertices[vIdx].position[1] = p.y; geom.vertices[vIdx].position[2] = p.z;
                                
                                glm::vec3 n = normalMat * glm::vec3(geom.vertices[vIdx].normal[0], geom.vertices[vIdx].normal[1], geom.vertices[vIdx].normal[2]);
                                if (glm::length(n) > 0.0001f) n = glm::normalize(n);
                                geom.vertices[vIdx].normal[0] = n.x; geom.vertices[vIdx].normal[1] = n.y; geom.vertices[vIdx].normal[2] = n.z;
                            }

                            if (explodeFactor > 0.01f) {
                                geom.vertices[vIdx].position[0] += explodeOffset.x;
                                geom.vertices[vIdx].position[1] += explodeOffset.y;
                                geom.vertices[vIdx].position[2] += explodeOffset.z;
                            }
                            shifted[vIdx] = true;
                        }
                    }
                }
            }
            
            for (const auto& v : geom.vertices) {
                for(int j=0; j<3; ++j) {
                    if (v.position[j] < newMin[j]) newMin[j] = v.position[j];
                    if (v.position[j] > newMax[j]) newMax[j] = v.position[j];
                }
            }
            m_masterVertices.insert(m_masterVertices.end(), geom.vertices.begin(), geom.vertices.end());
        }

        for (int j=0; j<3; ++j) {
            newMin[j] -= 0.1f; newMax[j] += 0.1f;
            if (newMax[j] - newMin[j] < 0.0001f) { newMin[j] -= 0.1f; newMax[j] += 0.1f; }
            minBounds[j] = newMin[j];
            maxBounds[j] = newMax[j];
            uiState.sceneMinBounds[j] = newMin[j];
            uiState.sceneMaxBounds[j] = newMax[j];
        }

        uiState.clipXMin = newMin[0] + pctXMin * (newMax[0] - newMin[0]);
        uiState.clipXMax = newMin[0] + pctXMax * (newMax[0] - newMin[0]);
        uiState.clipYMin = newMin[1] + pctYMin * (newMax[1] - newMin[1]);
        uiState.clipYMax = newMin[1] + pctYMax * (newMax[1] - newMin[1]);
        uiState.clipZMin = newMin[2] + pctZMin * (newMax[2] - newMin[2]);
        uiState.clipZMax = newMin[2] + pctZMax * (newMax[2] - newMin[2]);

        graphics->UpdateGeometry(m_masterVertices);
        uiState.updateGeometry = false;
    }

} // namespace BimCore