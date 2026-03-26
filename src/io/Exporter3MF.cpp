// =============================================================================
// BimCore/io/Exporter3MF.cpp
// =============================================================================
#include "io/Exporter3MF.h"
#include <iostream>
#include <sstream>

#include <miniz.h>
#include <pugixml.hpp>

namespace BimCore {

    bool Exporter3MF::Export(const std::string& filepath, std::shared_ptr<SceneModel> sourceModel) {
        if (!sourceModel) return false;

        auto& geom = sourceModel->GetGeometry();
        if (geom.vertices.empty() || geom.indices.empty()) {
            std::cerr << "[3MF Exporter] No geometry found to export.\n";
            return false;
        }

        // =====================================================================
        // 1. Bygg XML: /[Content_Types].xml
        // =====================================================================
        pugi::xml_document contentTypesDoc;
        pugi::xml_node typesNode = contentTypesDoc.append_child("Types");
        typesNode.append_attribute("xmlns") = "http://schemas.openxmlformats.org/package/2006/content-types";

        pugi::xml_node defaultRels = typesNode.append_child("Default");
        defaultRels.append_attribute("Extension") = "rels";
        defaultRels.append_attribute("ContentType") = "application/vnd.openxmlformats-package.relationships+xml";

        pugi::xml_node defaultModel = typesNode.append_child("Default");
        defaultModel.append_attribute("Extension") = "model";
        defaultModel.append_attribute("ContentType") = "application/vnd.ms-3dmanufacturing-3dmodel+xml";

        // =====================================================================
        // 2. Bygg XML: /_rels/.rels
        // =====================================================================
        pugi::xml_document relsDoc;
        pugi::xml_node relsNode = relsDoc.append_child("Relationships");
        relsNode.append_attribute("xmlns") = "http://schemas.openxmlformats.org/package/2006/relationships";

        pugi::xml_node relNode = relsNode.append_child("Relationship");
        relNode.append_attribute("Target") = "/3D/3dmodel.model";
        relNode.append_attribute("Id") = "rel0";
        relNode.append_attribute("Type") = "http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel";

        // =====================================================================
        // 3. Bygg XML: /3D/3dmodel.model
        // =====================================================================
        pugi::xml_document modelDoc;
        pugi::xml_node modelNode = modelDoc.append_child("model");
        modelNode.append_attribute("unit") = "millimeter"; // Standard i BIMCore?
        modelNode.append_attribute("xml:lang") = "en-US";
        modelNode.append_attribute("xmlns") = "http://schemas.microsoft.com/3dmanufacturing/core/2015/02";

        pugi::xml_node resourcesNode = modelNode.append_child("resources");
        pugi::xml_node objectNode = resourcesNode.append_child("object");
        objectNode.append_attribute("id") = "1";
        objectNode.append_attribute("type") = "model";

        pugi::xml_node meshNode = objectNode.append_child("mesh");
        
        // Vertices
        pugi::xml_node verticesNode = meshNode.append_child("vertices");
        for (const auto& v : geom.vertices) {
            pugi::xml_node vNode = verticesNode.append_child("vertex");
            vNode.append_attribute("x") = v.position[0];
            vNode.append_attribute("y") = v.position[1];
            vNode.append_attribute("z") = v.position[2];
        }

        // Triangles
        pugi::xml_node trianglesNode = meshNode.append_child("triangles");
        // Går gjennom indeksene med 3 om gangen for å forme trekanter
        for (size_t i = 0; i < geom.indices.size(); i += 3) {
            pugi::xml_node tNode = trianglesNode.append_child("triangle");
            tNode.append_attribute("v1") = geom.indices[i];
            tNode.append_attribute("v2") = geom.indices[i + 1];
            tNode.append_attribute("v3") = geom.indices[i + 2];
        }

        // Build Instructions (Hva som faktisk skal konstrueres/vises)
        pugi::xml_node buildNode = modelNode.append_child("build");
        pugi::xml_node itemNode = buildNode.append_child("item");
        itemNode.append_attribute("objectid") = "1";

        // Lambda-funksjon for å konvertere XML til std::string minne-buffer
        auto getXmlString = [](const pugi::xml_document& doc) {
            std::stringstream ss;
            doc.save(ss, "  ");
            return ss.str();
        };

        std::string contentTypesStr = getXmlString(contentTypesDoc);
        std::string relsStr = getXmlString(relsDoc);
        std::string modelStr = getXmlString(modelDoc);

        // =====================================================================
        // 4. Pakk alt inn i en ZIP-fil (3MF formatet)
        // =====================================================================
        mz_zip_archive zip_archive;
        memset(&zip_archive, 0, sizeof(zip_archive));

        if (!mz_zip_writer_init_file(&zip_archive, filepath.c_str(), 0)) {
            std::cerr << "[3MF Exporter] Failed to initialize zip file at: " << filepath << "\n";
            return false;
        }

        // Skriver strengene direkte inn i zip-filen i minnet
        mz_zip_writer_add_mem(&zip_archive, "[Content_Types].xml", contentTypesStr.c_str(), contentTypesStr.length(), MZ_DEFAULT_COMPRESSION);
        mz_zip_writer_add_mem(&zip_archive, "_rels/.rels", relsStr.c_str(), relsStr.length(), MZ_DEFAULT_COMPRESSION);
        mz_zip_writer_add_mem(&zip_archive, "3D/3dmodel.model", modelStr.c_str(), modelStr.length(), MZ_DEFAULT_COMPRESSION);

        mz_zip_writer_finalize_archive(&zip_archive);
        mz_zip_writer_end(&zip_archive);

        std::cout << "[3MF Exporter] Successfully exported " << (geom.indices.size() / 3) << " triangles to " << filepath << "\n";
        return true;
    }

} // namespace BimCore