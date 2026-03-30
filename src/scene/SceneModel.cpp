// =============================================================================
// BimCore/scene/SceneModel.cpp
// =============================================================================
#include "core/Core.h"
#include "scene/SceneModel.h"
#include <iostream>
#include <algorithm>
#include <execution>
#include <mutex>
#include <glm/gtc/matrix_transform.hpp>
#include <ifcparse/IfcFile.h>
#include <ifcparse/IfcGlobalId.h>

// Dynamically include all schemas the engine was compiled with
#ifdef HAS_SCHEMA_2x3
#include <ifcparse/Ifc2x3.h>
#endif
#ifdef HAS_SCHEMA_4
#include <ifcparse/Ifc4.h>
#endif
#ifdef HAS_SCHEMA_4x1
#include <ifcparse/Ifc4x1.h>
#endif
#ifdef HAS_SCHEMA_4x2
#include <ifcparse/Ifc4x2.h>
#endif
#ifdef HAS_SCHEMA_4x3
#include <ifcparse/Ifc4x3.h>
#endif
#ifdef HAS_SCHEMA_4x3_add1
#include <ifcparse/Ifc4x3_add1.h>
#endif
#ifdef HAS_SCHEMA_4x3_add2
#include <ifcparse/Ifc4x3_add2.h>
#endif

// =============================================================================
// MACRO DEFINITIONS FOR CROSS-SCHEMA SUPPORT
// Generates the identical logic for every minor IFC version automatically
// =============================================================================

#define INJECT_PSET_IFC4_FAMILY(SCHEMA_NS) \
if (auto objDef = targetObj->as<SCHEMA_NS::IfcObjectDefinition>()) { \
    auto newVal = new SCHEMA_NS::IfcPropertySingleValue(propName, boost::none, new SCHEMA_NS::IfcLabel(value), nullptr); \
    m_database->addEntity(newVal); \
    SCHEMA_NS::IfcPropertySet* targetPset = nullptr; \
    auto rels = m_database->instances_by_type("IfcRelDefinesByProperties"); \
    if (rels) { \
        for (auto rel : *rels) { \
            if (auto relTyped = rel->as<SCHEMA_NS::IfcRelDefinesByProperties>()) { \
                if (auto related = relTyped->RelatedObjects()) { \
                    bool isRelated = false; \
                    for (auto item : *related) { if (item == targetObj) { isRelated = true; break; } } \
                    if (isRelated) { \
                        if (auto psetDef = relTyped->RelatingPropertyDefinition()) { \
                            if (auto psetTyped = ((IfcUtil::IfcBaseClass*)psetDef)->as<SCHEMA_NS::IfcPropertySet>()) { \
                                if (psetTyped->Name() == psetName) { targetPset = psetTyped; break; } \
                            } \
                        } \
                    } \
                } \
            } \
        } \
    } \
    if (targetPset) { \
        if (auto currentProps = targetPset->HasProperties()) { \
            boost::shared_ptr<SCHEMA_NS::IfcProperty::list> newList(new SCHEMA_NS::IfcProperty::list()); \
            for (auto p : *currentProps) newList->push(p); \
            newList->push(newVal); \
            targetPset->setHasProperties(newList); \
        } \
    } else { \
        boost::shared_ptr<SCHEMA_NS::IfcProperty::list> propList(new SCHEMA_NS::IfcProperty::list()); \
        propList->push(newVal); \
        std::string newGuid = IfcParse::IfcGlobalId(); \
        auto newPset = new SCHEMA_NS::IfcPropertySet(newGuid, nullptr, psetName, boost::none, propList); \
        m_database->addEntity(newPset); \
        boost::shared_ptr<SCHEMA_NS::IfcObjectDefinition::list> objList(new SCHEMA_NS::IfcObjectDefinition::list()); \
        objList->push(objDef); \
        std::string relGuid = IfcParse::IfcGlobalId(); \
        auto newRel = new SCHEMA_NS::IfcRelDefinesByProperties(relGuid, nullptr, boost::none, boost::none, objList, newPset); \
        m_database->addEntity(newRel); \
    } \
    m_propertyCache.erase(guid); \
    LoadPropertiesFromAST(guid); \
    return true; \
}

#define INJECT_PSET_IFC2X3(SCHEMA_NS) \
if (auto objDef = targetObj->as<SCHEMA_NS::IfcObject>()) { \
    auto newVal = new SCHEMA_NS::IfcPropertySingleValue(propName, boost::none, new SCHEMA_NS::IfcLabel(value), nullptr); \
    m_database->addEntity(newVal); \
    SCHEMA_NS::IfcPropertySet* targetPset = nullptr; \
    auto rels = m_database->instances_by_type("IfcRelDefinesByProperties"); \
    if (rels) { \
        for (auto rel : *rels) { \
            if (auto relTyped = rel->as<SCHEMA_NS::IfcRelDefinesByProperties>()) { \
                if (auto related = relTyped->RelatedObjects()) { \
                    bool isRelated = false; \
                    for (auto item : *related) { if (item == targetObj) { isRelated = true; break; } } \
                    if (isRelated) { \
                        if (auto psetDef = relTyped->RelatingPropertyDefinition()) { \
                            if (auto psetTyped = ((IfcUtil::IfcBaseClass*)psetDef)->as<SCHEMA_NS::IfcPropertySet>()) { \
                                if (psetTyped->Name() == psetName) { targetPset = psetTyped; break; } \
                            } \
                        } \
                    } \
                } \
            } \
        } \
    } \
    if (targetPset) { \
        if (auto currentProps = targetPset->HasProperties()) { \
            boost::shared_ptr<SCHEMA_NS::IfcProperty::list> newList(new SCHEMA_NS::IfcProperty::list()); \
            for (auto p : *currentProps) newList->push(p); \
            newList->push(newVal); \
            targetPset->setHasProperties(newList); \
        } \
    } else { \
        boost::shared_ptr<SCHEMA_NS::IfcProperty::list> propList(new SCHEMA_NS::IfcProperty::list()); \
        propList->push(newVal); \
        std::string newGuid = IfcParse::IfcGlobalId(); \
        auto newPset = new SCHEMA_NS::IfcPropertySet(newGuid, nullptr, psetName, boost::none, propList); \
        m_database->addEntity(newPset); \
        boost::shared_ptr<SCHEMA_NS::IfcObject::list> objList(new SCHEMA_NS::IfcObject::list()); \
        objList->push(objDef); \
        std::string relGuid = IfcParse::IfcGlobalId(); \
        auto newRel = new SCHEMA_NS::IfcRelDefinesByProperties(relGuid, nullptr, boost::none, boost::none, objList, newPset); \
        m_database->addEntity(newRel); \
    } \
    m_propertyCache.erase(guid); \
    LoadPropertiesFromAST(guid); \
    return true; \
}

namespace BimCore {

    SceneModel::SceneModel(std::shared_ptr<IfcParse::IfcFile> database, RenderMesh geometry, const std::string& path)
    : m_database(database), m_geometry(geometry), m_filePath(path) {
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

        #ifdef HAS_SCHEMA_4x3_add2
        if (auto root = targetObj->as<Ifc4x3_add2::IfcRoot>()) {
            if (root->Name()) { std::string nameStr = root->Name().get(); props["Name"] = {nameStr, nameStr, false, false}; }
        }
        #endif
        #ifdef HAS_SCHEMA_4x3_add1
        if (auto root = targetObj->as<Ifc4x3_add1::IfcRoot>()) {
            if (root->Name()) { std::string nameStr = root->Name().get(); props["Name"] = {nameStr, nameStr, false, false}; }
        }
        #endif
        #ifdef HAS_SCHEMA_4x3
        if (auto root = targetObj->as<Ifc4x3::IfcRoot>()) {
            if (root->Name()) { std::string nameStr = root->Name().get(); props["Name"] = {nameStr, nameStr, false, false}; }
        } 
        #endif
        #ifdef HAS_SCHEMA_4
        if (auto root = targetObj->as<Ifc4::IfcRoot>()) {
            if (root->Name()) { std::string nameStr = root->Name().get(); props["Name"] = {nameStr, nameStr, false, false}; }
        } 
        #endif
        #ifdef HAS_SCHEMA_2x3
        if (auto root = targetObj->as<Ifc2x3::IfcRoot>()) {
            if (root->Name()) { std::string nameStr = root->Name().get(); props["Name"] = {nameStr, nameStr, false, false}; }
        }
        #endif

        auto rels = m_database->instances_by_type("IfcRelDefinesByProperties");
        if (rels) {
            for (auto it = rels->begin(); it != rels->end(); ++it) {
                IfcUtil::IfcBaseClass* rel = *it;
                #ifdef HAS_SCHEMA_4
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
                        }
                    }
                } 
                #endif
                #ifdef HAS_SCHEMA_4x3_add2
                if (auto rel4x3 = rel->as<Ifc4x3_add2::IfcRelDefinesByProperties>()) {
                    auto related = rel4x3->RelatedObjects();
                    if (!related) continue;
                    bool isRelated = false;
                    for (auto obj : *related) { if (obj == targetObj) { isRelated = true; break; } }
                    if (isRelated) {
                        auto psetSelect = rel4x3->RelatingPropertyDefinition();
                        if (!psetSelect) continue;
                        IfcUtil::IfcBaseClass* baseClass = (IfcUtil::IfcBaseClass*)psetSelect;
                        if (auto pset4 = baseClass->as<Ifc4x3_add2::IfcPropertySet>()) {
                            if (auto hasProps = pset4->HasProperties()) {
                                for (auto prop : *hasProps) {
                                    if (auto psv = prop->as<Ifc4x3_add2::IfcPropertySingleValue>()) {
                                        std::string name = psv->Name();
                                        std::string val = "";
                                        if (auto nominal = psv->NominalValue()) {
                                            if (nominal->declaration().is("IfcLabel")) val = (std::string)*(nominal->as<Ifc4x3_add2::IfcLabel>());
                                            else if (nominal->declaration().is("IfcText")) val = (std::string)*(nominal->as<Ifc4x3_add2::IfcText>());
                                            else if (nominal->declaration().is("IfcIdentifier")) val = (std::string)*(nominal->as<Ifc4x3_add2::IfcIdentifier>());
                                            else if (nominal->declaration().is("IfcReal")) val = std::to_string((double)*(nominal->as<Ifc4x3_add2::IfcReal>()));
                                            else if (nominal->declaration().is("IfcBoolean")) val = ((bool)*(nominal->as<Ifc4x3_add2::IfcBoolean>())) ? "True" : "False";
                                            else val = "[" + nominal->declaration().name() + "]";
                                        }
                                        props[name] = {val, val, false, false};
                                    }
                                }
                            }
                        }
                    }
                } 
                #endif
                #ifdef HAS_SCHEMA_2x3
                if (auto rel2 = rel->as<Ifc2x3::IfcRelDefinesByProperties>()) {
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
                        }
                    }
                }
                #endif
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
            #ifdef HAS_SCHEMA_4x3_add2
            if (auto root = obj->as<Ifc4x3_add2::IfcRoot>()) { if (root->Name()) return root->Name().get(); }
            #endif
            #ifdef HAS_SCHEMA_4
            if (auto root = obj->as<Ifc4::IfcRoot>()) { if (root->Name()) return root->Name().get(); }
            #endif
            #ifdef HAS_SCHEMA_2x3
            if (auto root = obj->as<Ifc2x3::IfcRoot>()) { if (root->Name()) return root->Name().get(); }
            #endif
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
        if (m_propertyCache[guid].find(propName) == m_propertyCache[guid].end()) {
            m_propertyCache[guid][propName] = { "", "", false, false };
        }
        m_propertyCache[guid][propName].value = newValue;
        m_propertyCache[guid][propName].isModified = (newValue != m_propertyCache[guid][propName].originalValue);
        m_propertyCache[guid][propName].isDeleted = false;
        return true;
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

    // =========================================================================
    // CUSTOM PROPERTY INJECTION ENGINE
    // Automatically routes to the exact schema namespace discovered at runtime
    // =========================================================================
    bool SceneModel::AddCustomProperty(const std::string& guid, const std::string& psetName, const std::string& propName, const std::string& value) {
        if (!m_database) return false;

        std::string realGuid = guid.length() >= 22 ? guid.substr(0, 22) : guid;
        IfcUtil::IfcBaseClass* targetObj = nullptr;
        try { targetObj = m_database->instance_by_guid(realGuid); } catch(...) { return false; }
        if (!targetObj) return false;

        std::string schema = m_database->schema()->name();

        if (schema == "IFC4X3_ADD2") {
            #ifdef HAS_SCHEMA_4x3_add2
            INJECT_PSET_IFC4_FAMILY(Ifc4x3_add2)
            #endif
        } else if (schema == "IFC4X3_ADD1") {
            #ifdef HAS_SCHEMA_4x3_add1
            INJECT_PSET_IFC4_FAMILY(Ifc4x3_add1)
            #endif
        } else if (schema == "IFC4X3") {
            #ifdef HAS_SCHEMA_4x3
            INJECT_PSET_IFC4_FAMILY(Ifc4x3)
            #endif
        } else if (schema == "IFC4X2") {
            #ifdef HAS_SCHEMA_4x2
            INJECT_PSET_IFC4_FAMILY(Ifc4x2)
            #endif
        } else if (schema == "IFC4X1") {
            #ifdef HAS_SCHEMA_4x1
            INJECT_PSET_IFC4_FAMILY(Ifc4x1)
            #endif
        } else if (schema == "IFC4") {
            #ifdef HAS_SCHEMA_4
            INJECT_PSET_IFC4_FAMILY(Ifc4)
            #endif
        } else if (schema == "IFC2X3") {
            #ifdef HAS_SCHEMA_2x3
            INJECT_PSET_IFC2X3(Ifc2x3)
            #endif
        }

        return false;
    }

    bool SceneModel::DeleteElement(const std::string& guid) {
        if (!m_database) return false;
        try {
            std::string realGuid = guid.length() >= 22 ? guid.substr(0, 22) : guid;
            IfcUtil::IfcBaseClass* obj = m_database->instance_by_guid(realGuid);
            if (obj) {
                m_database->removeEntity(obj);
            }
        } catch (...) {}

        m_geometry.subMeshes.erase(
            std::remove_if(m_geometry.subMeshes.begin(), m_geometry.subMeshes.end(),
                           [&](const RenderSubMesh& sub) { return sub.guid == guid; }),
                                   m_geometry.subMeshes.end()
        );

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
            for (auto& [key, info] : props) { 
                if (info.isModified || info.isDeleted) { needsASTUpdate = true; break; } 
            }
            if (!needsASTUpdate) continue;
            
            std::string realGuid = guid.length() >= 22 ? guid.substr(0, 22) : guid;
            IfcUtil::IfcBaseClass* targetObj = nullptr;
            try { targetObj = m_database->instance_by_guid(realGuid); } catch(...) { continue; }
            if (!targetObj) continue;
            
            if (props.count("Name")) {
                auto& info = props["Name"];
                if (info.isModified) {
                    #ifdef HAS_SCHEMA_4x3_add2
                    if (auto root = targetObj->as<Ifc4x3_add2::IfcRoot>()) { root->setName(boost::optional<std::string>(info.value)); }
                    #endif
                    #ifdef HAS_SCHEMA_4
                    if (auto root = targetObj->as<Ifc4::IfcRoot>()) { root->setName(boost::optional<std::string>(info.value)); }
                    #endif
                    #ifdef HAS_SCHEMA_2x3
                    if (auto root = targetObj->as<Ifc2x3::IfcRoot>()) { root->setName(boost::optional<std::string>(info.value)); }
                    #endif
                }
            }

            auto rels = m_database->instances_by_type("IfcRelDefinesByProperties");
            if (!rels) continue;
            for (auto it = rels->begin(); it != rels->end(); ++it) {
                IfcUtil::IfcBaseClass* rel = *it;
                #ifdef HAS_SCHEMA_4
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
                        }
                    }
                } 
                #endif
                #ifdef HAS_SCHEMA_4x3_add2
                if (auto rel4x3 = rel->as<Ifc4x3_add2::IfcRelDefinesByProperties>()) {
                    auto related = rel4x3->RelatedObjects();
                    if (!related) continue;
                    bool isRelated = false;
                    for (auto obj : *related) { if (obj == targetObj) { isRelated = true; break; } }
                    if (isRelated) {
                        auto psetSelect = rel4x3->RelatingPropertyDefinition();
                        if (!psetSelect) continue;
                        if (auto pset4 = ((IfcUtil::IfcBaseClass*)psetSelect)->as<Ifc4x3_add2::IfcPropertySet>()) {
                            if (auto hasProps = pset4->HasProperties()) {
                                for (auto prop : *hasProps) {
                                    if (auto psv = prop->as<Ifc4x3_add2::IfcPropertySingleValue>()) {
                                        std::string pName = psv->Name();
                                        if (props.find(pName) != props.end()) {
                                            auto& info = props[pName];
                                            if (info.isDeleted) entitiesToDelete.push_back(psv);
                                            else if (info.isModified) psv->setNominalValue(new Ifc4x3_add2::IfcLabel(info.value));
                                        }
                                    }
                                }
                            }
                        }
                    }
                } 
                #endif
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

        std::for_each(std::execution::par_unseq,
            m_geometry.originalVertices.begin(),
            m_geometry.originalVertices.end(),
            [&matrix, &normalMatrix](auto& v) {
                glm::vec4 p = matrix * glm::vec4(v.position[0], v.position[1], v.position[2], 1.0f);
                v.position[0] = p.x; v.position[1] = p.y; v.position[2] = p.z;

                glm::vec3 n = normalMatrix * glm::vec3(v.normal[0], v.normal[1], v.normal[2]);
                n = glm::normalize(n);
                v.normal[0] = n.x; v.normal[1] = n.y; v.normal[2] = n.z;
            }
        );

        m_geometry.vertices = m_geometry.originalVertices;

        float minX = 1e9f, minY = 1e9f, minZ = 1e9f;
        float maxX = -1e9f, maxY = -1e9f, maxZ = -1e9f;

        for (const auto& v : m_geometry.vertices) {
            if (v.position[0] < minX) minX = v.position[0];
            if (v.position[0] > maxX) maxX = v.position[0];
            if (v.position[1] < minY) minY = v.position[1];
            if (v.position[1] > maxY) maxY = v.position[1];
            if (v.position[2] < minZ) minZ = v.position[2];
            if (v.position[2] > maxZ) maxZ = v.position[2];
        }

        m_geometry.minBounds[0] = minX; m_geometry.minBounds[1] = minY; m_geometry.minBounds[2] = minZ;
        m_geometry.maxBounds[0] = maxX; m_geometry.maxBounds[1] = maxY; m_geometry.maxBounds[2] = maxZ;

        for (int j=0; j<3; ++j) m_geometry.center[j] = (m_geometry.minBounds[j] + m_geometry.maxBounds[j]) * 0.5f;

        std::for_each(std::execution::par_unseq,
            m_geometry.subMeshes.begin(),
            m_geometry.subMeshes.end(),
            [&matrix](auto& sub) {
                glm::vec4 cp = matrix * glm::vec4(sub.center[0], sub.center[1], sub.center[2], 1.0f);
                sub.center[0] = cp.x; sub.center[1] = cp.y; sub.center[2] = cp.z;
            }
        );
    }

} // namespace BimCore