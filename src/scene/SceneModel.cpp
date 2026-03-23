// =============================================================================
// BimCore/scene/SceneModel.cpp
// =============================================================================
#include "core/Core.h"
#include "scene/SceneModel.h"
#include <iostream>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
// Strictly using early-bound generated C++ schemas for maximum type safety.
#include <ifcparse/IfcFile.h>
#include <ifcparse/Ifc2x3.h>
#include <ifcparse/Ifc4.h>

namespace BimCore {

    SceneModel::SceneModel(std::shared_ptr<IfcParse::IfcFile> database, RenderMesh geometry, const std::string& path)
    : m_database(database), m_geometry(geometry), m_filePath(path) {
        // --- NEW: Build the groups immediately upon load to stop UI heap churn ---
        BuildUIGroups();
    }

    void SceneModel::BuildUIGroups() {
        m_uiGroups.clear();
        for (uint32_t i = 0; i < m_geometry.subMeshes.size(); ++i) {
            m_uiGroups[m_geometry.subMeshes[i].type].push_back(i);
        }
    }

    RenderMesh& SceneModel::GetGeometry() { return m_geometry; }
    std::shared_ptr<IfcParse::IfcFile> SceneModel::GetDatabase() { return m_database; }
    std::string SceneModel::GetFilePath() const { return m_filePath; }

    void SceneModel::LoadPropertiesFromAST(const std::string& guid) {
        if (!m_database || m_propertyCache.find(guid) != m_propertyCache.end()) return;

        std::map<std::string, PropertyInfo> props;
        std::string realGuid = guid.length() >= 22 ? guid.substr(0, 22) : guid;

        IfcUtil::IfcBaseClass* targetObj = nullptr;
        try { targetObj = m_database->instance_by_guid(realGuid); } catch (...) { m_propertyCache[guid] = props; return; }
        if (!targetObj) { m_propertyCache[guid] = props; return; }

        if (auto root4 = targetObj->as<Ifc4::IfcRoot>()) {
            if (root4->Name()) {
                std::string nameStr = root4->Name().get();
                props["Name"] = {nameStr, nameStr, false, false};
            }
        } else if (auto root2 = targetObj->as<Ifc2x3::IfcRoot>()) {
            if (root2->Name()) {
                std::string nameStr = root2->Name().get();
                props["Name"] = {nameStr, nameStr, false, false};
            }
        }

        auto rels = m_database->instances_by_type("IfcRelDefinesByProperties");
        if (rels) {
            for (auto it = rels->begin(); it != rels->end(); ++it) {
                IfcUtil::IfcBaseClass* rel = *it;
                if (auto rel4 = rel->as<Ifc4::IfcRelDefinesByProperties>()) {
                    auto related = rel4->RelatedObjects();
                    if (!related) continue;
                    bool isRelated = false;
                    for (auto obj : *related) { if (obj == targetObj) { isRelated = true; break; } }
                    if (isRelated) {
                        auto psetSelect = rel4->RelatingPropertyDefinition();
                        if (!psetSelect) continue;
                        IfcUtil::IfcBaseClass* baseClass = (IfcUtil::IfcBaseClass*)psetSelect;
                        if (auto pset4 = baseClass->as<Ifc4::IfcPropertySet>()) {
                            if (auto hasProps = pset4->HasProperties()) {
                                for (auto prop : *hasProps) {
                                    if (auto psv = prop->as<Ifc4::IfcPropertySingleValue>()) {
                                        std::string name = psv->Name();
                                        std::string val = "";
                                        if (auto nominal = psv->NominalValue()) {
                                            if (nominal->declaration().is("IfcLabel")) val = (std::string)*(nominal->as<Ifc4::IfcLabel>());
                                            else if (nominal->declaration().is("IfcText")) val = (std::string)*(nominal->as<Ifc4::IfcText>());
                                            else if (nominal->declaration().is("IfcIdentifier")) val = (std::string)*(nominal->as<Ifc4::IfcIdentifier>());
                                            else if (nominal->declaration().is("IfcReal")) val = std::to_string((double)*(nominal->as<Ifc4::IfcReal>()));
                                            else if (nominal->declaration().is("IfcBoolean")) val = ((bool)*(nominal->as<Ifc4::IfcBoolean>())) ? "True" : "False";
                                            else val = "[" + nominal->declaration().name() + "]";
                                        }
                                        props[name] = {val, val, false, false};
                                    }
                                }
                            }
                        } else if (auto qset4 = baseClass->as<Ifc4::IfcElementQuantity>()) {
                            if (auto quantities = qset4->Quantities()) {
                                for (auto q : *quantities) {
                                    if (auto qLength = q->as<Ifc4::IfcQuantityLength>()) props[qLength->Name()] = {std::to_string(qLength->LengthValue()), std::to_string(qLength->LengthValue()), false, false};
                                    else if (auto qArea = q->as<Ifc4::IfcQuantityArea>()) props[qArea->Name()] = {std::to_string(qArea->AreaValue()), std::to_string(qArea->AreaValue()), false, false};
                                    else if (auto qVol = q->as<Ifc4::IfcQuantityVolume>()) props[qVol->Name()] = {std::to_string(qVol->VolumeValue()), std::to_string(qVol->VolumeValue()), false, false};
                                    else if (auto qWeight = q->as<Ifc4::IfcQuantityWeight>()) props[qWeight->Name()] = {std::to_string(qWeight->WeightValue()), std::to_string(qWeight->WeightValue()), false, false};
                                }
                            }
                        }
                    }
                } else if (auto rel2 = rel->as<Ifc2x3::IfcRelDefinesByProperties>()) {
                    auto related = rel2->RelatedObjects();
                    if (!related) continue;
                    bool isRelated = false;
                    for (auto obj : *related) { if (obj == targetObj) { isRelated = true; break; } }
                    if (isRelated) {
                        auto psetSelect = rel2->RelatingPropertyDefinition();
                        if (!psetSelect) continue;
                        IfcUtil::IfcBaseClass* baseClass = (IfcUtil::IfcBaseClass*)psetSelect;
                        if (auto pset2 = baseClass->as<Ifc2x3::IfcPropertySet>()) {
                            if (auto hasProps = pset2->HasProperties()) {
                                for (auto prop : *hasProps) {
                                    if (auto psv = prop->as<Ifc2x3::IfcPropertySingleValue>()) {
                                        std::string name = psv->Name();
                                        std::string val = "";
                                        if (auto nominal = psv->NominalValue()) {
                                            if (nominal->declaration().is("IfcLabel")) val = (std::string)*(nominal->as<Ifc2x3::IfcLabel>());
                                            else if (nominal->declaration().is("IfcText")) val = (std::string)*(nominal->as<Ifc2x3::IfcText>());
                                            else if (nominal->declaration().is("IfcIdentifier")) val = (std::string)*(nominal->as<Ifc2x3::IfcIdentifier>());
                                            else if (nominal->declaration().is("IfcReal")) val = std::to_string((double)*(nominal->as<Ifc2x3::IfcReal>()));
                                            else if (nominal->declaration().is("IfcBoolean")) val = ((bool)*(nominal->as<Ifc2x3::IfcBoolean>())) ? "True" : "False";
                                            else val = "[" + nominal->declaration().name() + "]";
                                        }
                                        props[name] = {val, val, false, false};
                                    }
                                }
                            }
                        } else if (auto qset2 = baseClass->as<Ifc2x3::IfcElementQuantity>()) {
                            if (auto quantities = qset2->Quantities()) {
                                for (auto q : *quantities) {
                                    if (auto qLength = q->as<Ifc2x3::IfcQuantityLength>()) props[qLength->Name()] = {std::to_string(qLength->LengthValue()), std::to_string(qLength->LengthValue()), false, false};
                                    else if (auto qArea = q->as<Ifc2x3::IfcQuantityArea>()) props[qArea->Name()] = {std::to_string(qArea->AreaValue()), std::to_string(qArea->AreaValue()), false, false};
                                    else if (auto qVol = q->as<Ifc2x3::IfcQuantityVolume>()) props[qVol->Name()] = {std::to_string(qVol->VolumeValue()), std::to_string(qVol->VolumeValue()), false, false};
                                    else if (auto qWeight = q->as<Ifc2x3::IfcQuantityWeight>()) props[qWeight->Name()] = {std::to_string(qWeight->WeightValue()), std::to_string(qWeight->WeightValue()), false, false};
                                }
                            }
                        }
                    }
                }
            }
        }

        for (const auto& sub : m_geometry.subMeshes) {
            if (sub.guid == guid) {
                props["Parent IFC GUID"] = {realGuid, realGuid, false, false};
                props["Triangle Count"] = {std::to_string(sub.indexCount / 3), std::to_string(sub.indexCount / 3), false, false};
                if (sub.indexCount > 0) {
                    uint32_t firstV = m_geometry.indices[sub.startIndex];
                    char hexColor[16]; snprintf(hexColor, sizeof(hexColor), "#%02X%02X%02X",
                                                (int)(m_geometry.vertices[firstV].color[0] * 255),
                                                (int)(m_geometry.vertices[firstV].color[1] * 255),
                                                (int)(m_geometry.vertices[firstV].color[2] * 255));
                    props["Live Geometry Color"] = {hexColor, hexColor, false, false};
                }
                break;
            }
        }
        m_propertyCache[guid] = props;
    }

    std::map<std::string, PropertyInfo> SceneModel::GetElementProperties(const std::string& guid) {
        LoadPropertiesFromAST(guid);
        return m_propertyCache[guid];
    }

    std::string SceneModel::GetElementNameFast(const std::string& guid) {
        auto it = m_propertyCache.find(guid);
        if (it != m_propertyCache.end() && it->second.find("Name") != it->second.end()) {
            return it->second["Name"].value;
        }
        std::string realGuid = guid.length() >= 22 ? guid.substr(0, 22) : guid;
        IfcUtil::IfcBaseClass* obj = nullptr;
        try { obj = m_database->instance_by_guid(realGuid); } catch(...) { return "Unknown"; }
        if (obj) {
            if (auto root4 = obj->as<Ifc4::IfcRoot>()) { if (root4->Name()) return root4->Name().get(); }
            else if (auto root2 = obj->as<Ifc2x3::IfcRoot>()) { if (root2->Name()) return root2->Name().get(); }
        }
        return "Unknown";
    }

    bool SceneModel::HasModifiedProperties(const std::string& guid) const {
        auto it = m_propertyCache.find(guid);
        if (it != m_propertyCache.end()) {
            for (const auto& pair : it->second) { if (pair.second.isModified || pair.second.isDeleted) return true; }
        }
        return false;
    }

    bool SceneModel::UpdateElementProperty(const std::string& guid, const std::string& propName, const std::string& newValue) {
        LoadPropertiesFromAST(guid);
        if (m_propertyCache[guid].find(propName) != m_propertyCache[guid].end()) {
            m_propertyCache[guid][propName].value = newValue;
            m_propertyCache[guid][propName].isModified = (newValue != m_propertyCache[guid][propName].originalValue);
            m_propertyCache[guid][propName].isDeleted = false;
            return true;
        }
        return false;
    }

    bool SceneModel::DeleteElementProperty(const std::string& guid, const std::string& propName) {
        LoadPropertiesFromAST(guid);
        if (m_propertyCache[guid].find(propName) != m_propertyCache[guid].end()) {
            m_propertyCache[guid][propName].isDeleted = true;
            return true;
        }
        return false;
    }

    bool SceneModel::UndoElementProperty(const std::string& guid, const std::string& propName) {
        LoadPropertiesFromAST(guid);
        if (m_propertyCache[guid].find(propName) != m_propertyCache[guid].end()) {
            m_propertyCache[guid][propName].value = m_propertyCache[guid][propName].originalValue;
            m_propertyCache[guid][propName].isModified = false;
            m_propertyCache[guid][propName].isDeleted = false;
            return true;
        }
        return false;
    }

    bool SceneModel::DeleteElement(const std::string& guid) {
        if (!m_database) return false;
        try {
            std::string realGuid = guid.length() >= 22 ? guid.substr(0, 22) : guid;
            IfcUtil::IfcBaseClass* obj = m_database->instance_by_guid(realGuid);
            if (obj) m_database->removeEntity(obj);
        } catch (...) {}

        m_geometry.subMeshes.erase(
            std::remove_if(m_geometry.subMeshes.begin(), m_geometry.subMeshes.end(),
                           [&](const RenderSubMesh& sub) { return sub.guid == guid; }),
                                   m_geometry.subMeshes.end()
        );

        // --- NEW: Rebuild the UI groups since the indices just shifted due to deletion ---
        BuildUIGroups();

        return true;
    }

    bool SceneModel::UpdateElementColor(const std::string& guid, float r, float g, float b) {
        if (!m_database) return false;
        try {
            for (auto& sub : m_geometry.subMeshes) {
                if (sub.guid == guid) {
                    for (size_t i = 0; i < sub.indexCount; ++i) {
                        uint32_t vIdx = m_geometry.indices[sub.startIndex + i];
                        m_geometry.vertices[vIdx].color[0] = r;
                        m_geometry.vertices[vIdx].color[1] = g;
                        m_geometry.vertices[vIdx].color[2] = b;
                    }
                    return true;
                }
            }
        } catch (...) {}
        return false;
    }

    bool SceneModel::CommitASTChanges() {
        if (!m_database) return false;
        std::vector<IfcUtil::IfcBaseClass*> entitiesToDelete;
        for (auto& [guid, props] : m_propertyCache) {
            bool needsASTUpdate = false;
            for (auto& [key, info] : props) { if (info.isModified || info.isDeleted) { needsASTUpdate = true; break; } }
            if (!needsASTUpdate) continue;
            std::string realGuid = guid.length() >= 22 ? guid.substr(0, 22) : guid;
            IfcUtil::IfcBaseClass* targetObj = nullptr;
            try { targetObj = m_database->instance_by_guid(realGuid); } catch(...) { continue; }
            if (!targetObj) continue;
            if (props.count("Name")) {
                auto& info = props["Name"];
                if (info.isModified) {
                    if (auto root4 = targetObj->as<Ifc4::IfcRoot>()) root4->setName(info.value);
                    else if (auto root2 = targetObj->as<Ifc2x3::IfcRoot>()) root2->setName(info.value);
                }
            }
            auto rels = m_database->instances_by_type("IfcRelDefinesByProperties");
            if (!rels) continue;
            for (auto it = rels->begin(); it != rels->end(); ++it) {
                IfcUtil::IfcBaseClass* rel = *it;
                if (auto rel4 = rel->as<Ifc4::IfcRelDefinesByProperties>()) {
                    auto related = rel4->RelatedObjects();
                    if (!related) continue;
                    bool isRelated = false;
                    for (auto obj : *related) { if (obj == targetObj) { isRelated = true; break; } }
                    if (isRelated) {
                        auto psetSelect = rel4->RelatingPropertyDefinition();
                        if (!psetSelect) continue;
                        if (auto pset4 = ((IfcUtil::IfcBaseClass*)psetSelect)->as<Ifc4::IfcPropertySet>()) {
                            if (auto hasProps = pset4->HasProperties()) {
                                for (auto prop : *hasProps) {
                                    if (auto psv = prop->as<Ifc4::IfcPropertySingleValue>()) {
                                        std::string pName = psv->Name();
                                        if (props.find(pName) != props.end()) {
                                            auto& info = props[pName];
                                            if (info.isDeleted) entitiesToDelete.push_back(psv);
                                            else if (info.isModified) psv->setNominalValue(new Ifc4::IfcLabel(info.value));
                                        }
                                    }
                                }
                            }
                        } else if (auto qset4 = ((IfcUtil::IfcBaseClass*)psetSelect)->as<Ifc4::IfcElementQuantity>()) {
                            if (auto quantities = qset4->Quantities()) {
                                for (auto q : *quantities) {
                                    if (auto qLength = q->as<Ifc4::IfcQuantityLength>()) {
                                        std::string qName = qLength->Name();
                                        if (props.count(qName)) {
                                            if (props[qName].isDeleted) entitiesToDelete.push_back(qLength);
                                            else if (props[qName].isModified) { try { qLength->setLengthValue(std::stod(props[qName].value)); } catch(...) {} }
                                        }
                                    } else if (auto qArea = q->as<Ifc4::IfcQuantityArea>()) {
                                        std::string qName = qArea->Name();
                                        if (props.count(qName)) {
                                            if (props[qName].isDeleted) entitiesToDelete.push_back(qArea);
                                            else if (props[qName].isModified) { try { qArea->setAreaValue(std::stod(props[qName].value)); } catch(...) {} }
                                        }
                                    } else if (auto qVol = q->as<Ifc4::IfcQuantityVolume>()) {
                                        std::string qName = qVol->Name();
                                        if (props.count(qName)) {
                                            if (props[qName].isDeleted) entitiesToDelete.push_back(qVol);
                                            else if (props[qName].isModified) { try { qVol->setVolumeValue(std::stod(props[qName].value)); } catch(...) {} }
                                        }
                                    } else if (auto qWeight = q->as<Ifc4::IfcQuantityWeight>()) {
                                        std::string qName = qWeight->Name();
                                        if (props.count(qName)) {
                                            if (props[qName].isDeleted) entitiesToDelete.push_back(qWeight);
                                            else if (props[qName].isModified) { try { qWeight->setWeightValue(std::stod(props[qName].value)); } catch(...) {} }
                                        }
                                    }
                                }
                            }
                        }
                    }
                } else if (auto rel2 = rel->as<Ifc2x3::IfcRelDefinesByProperties>()) {
                    auto related = rel2->RelatedObjects();
                    if (!related) continue;
                    bool isRelated = false;
                    for (auto obj : *related) { if (obj == targetObj) { isRelated = true; break; } }
                    if (isRelated) {
                        auto psetSelect = rel2->RelatingPropertyDefinition();
                        if (!psetSelect) continue;
                        if (auto pset2 = ((IfcUtil::IfcBaseClass*)psetSelect)->as<Ifc2x3::IfcPropertySet>()) {
                            if (auto hasProps = pset2->HasProperties()) {
                                for (auto prop : *hasProps) {
                                    if (auto psv = prop->as<Ifc2x3::IfcPropertySingleValue>()) {
                                        std::string pName = psv->Name();
                                        if (props.find(pName) != props.end()) {
                                            auto& info = props[pName];
                                            if (info.isDeleted) entitiesToDelete.push_back(psv);
                                            else if (info.isModified) psv->setNominalValue(new Ifc2x3::IfcLabel(info.value));
                                        }
                                    }
                                }
                            }
                        } else if (auto qset2 = ((IfcUtil::IfcBaseClass*)psetSelect)->as<Ifc2x3::IfcElementQuantity>()) {
                            if (auto quantities = qset2->Quantities()) {
                                for (auto q : *quantities) {
                                    if (auto qLength = q->as<Ifc2x3::IfcQuantityLength>()) {
                                        std::string qName = qLength->Name();
                                        if (props.count(qName)) {
                                            if (props[qName].isDeleted) entitiesToDelete.push_back(qLength);
                                            else if (props[qName].isModified) { try { qLength->setLengthValue(std::stod(props[qName].value)); } catch(...) {} }
                                        }
                                    } else if (auto qArea = q->as<Ifc2x3::IfcQuantityArea>()) {
                                        std::string qName = qArea->Name();
                                        if (props.count(qName)) {
                                            if (props[qName].isDeleted) entitiesToDelete.push_back(qArea);
                                            else if (props[qName].isModified) { try { qArea->setAreaValue(std::stod(props[qName].value)); } catch(...) {} }
                                        }
                                    } else if (auto qVol = q->as<Ifc2x3::IfcQuantityVolume>()) {
                                        std::string qName = qVol->Name();
                                        if (props.count(qName)) {
                                            if (props[qName].isDeleted) entitiesToDelete.push_back(qVol);
                                            else if (props[qName].isModified) { try { qVol->setVolumeValue(std::stod(props[qName].value)); } catch(...) {} }
                                        }
                                    } else if (auto qWeight = q->as<Ifc2x3::IfcQuantityWeight>()) {
                                        std::string qName = qWeight->Name();
                                        if (props.count(qName)) {
                                            if (props[qName].isDeleted) entitiesToDelete.push_back(qWeight);
                                            else if (props[qName].isModified) { try { qWeight->setWeightValue(std::stod(props[qName].value)); } catch(...) {} }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        for (auto* obj : entitiesToDelete) { try { m_database->removeEntity(obj); } catch(...) {} }

        for (auto& [guid, props] : m_propertyCache) {
            for (auto it = props.begin(); it != props.end(); ) {
                if (it->second.isDeleted) {
                    it = props.erase(it);
                } else {
                    it->second.isModified = false;
                    it->second.originalValue = it->second.value;
                    ++it;
                }
            }
        }

        BIM_LOG("Document", "AST Commit Complete. Database synchronized with Ledger.");
        return true;
    }

    void SceneModel::ApplyTransform(const glm::mat4& matrix) {
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(matrix)));

        for (auto& v : m_geometry.originalVertices) {
            glm::vec4 p = matrix * glm::vec4(v.position[0], v.position[1], v.position[2], 1.0f);
            v.position[0] = p.x; v.position[1] = p.y; v.position[2] = p.z;

            glm::vec3 n = normalMatrix * glm::vec3(v.normal[0], v.normal[1], v.normal[2]);
            n = glm::normalize(n);
            v.normal[0] = n.x; v.normal[1] = n.y; v.normal[2] = n.z;
        }

        m_geometry.vertices = m_geometry.originalVertices;

        for (int j=0; j<3; ++j) { m_geometry.minBounds[j] = 1e9f; m_geometry.maxBounds[j] = -1e9f; }
        for (const auto& v : m_geometry.vertices) {
            for (int j=0; j<3; ++j) {
                if (v.position[j] < m_geometry.minBounds[j]) m_geometry.minBounds[j] = v.position[j];
                if (v.position[j] > m_geometry.maxBounds[j]) m_geometry.maxBounds[j] = v.position[j];
            }
        }
        for (int j=0; j<3; ++j) m_geometry.center[j] = (m_geometry.minBounds[j] + m_geometry.maxBounds[j]) * 0.5f;

        for (auto& sub : m_geometry.subMeshes) {
            glm::vec4 cp = matrix * glm::vec4(sub.center[0], sub.center[1], sub.center[2], 1.0f);
            sub.center[0] = cp.x; sub.center[1] = cp.y; sub.center[2] = cp.z;
        }
    }

}