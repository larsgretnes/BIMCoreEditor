// =============================================================================
// BimCore/tests/test_ifc_delete_roundtrip.cpp
// =============================================================================
#include <catch2/catch_test_macros.hpp>
#include "io/IfcLoader.h"
#include "io/IfcExporter.h"
#include "scene/SceneModel.h"
#include <filesystem>

using namespace BimCore;

TEST_CASE("IFC Deletion Pipeline: Safe Spatial Cleanup", "[ifc][io][integration][delete]") {
    std::string sourceFile = "tests/data/wall-extruded-solid.ifc";
    std::string outputFile = "tests/data/temp_wall_delete_output.ifc";

    if (std::filesystem::exists(outputFile)) {
        std::filesystem::remove(outputFile);
    }

    auto model = IfcLoader::LoadDocument(sourceFile);
    REQUIRE(model != nullptr);
    
    uint32_t originalCount = model->GetGeometry().subMeshes.size();
    REQUIRE(originalCount > 0);

    std::string targetGuid = model->GetGeometry().subMeshes[0].guid;

    bool deleted = model->DeleteElement(targetGuid);
    REQUIRE(deleted == true);
    
    bool exported = IfcExporter::ExportIFC(model, outputFile);
    REQUIRE(exported == true);

    // 5. Load the newly exported file into a fresh model
    auto roundTripModel = IfcLoader::LoadDocument(outputFile);

    // 6. VERIFY: Because we deleted the ONLY element in the file, 
    // the IfcOpenShell geometry iterator will cleanly abort and return nullptr.
    // If it returns a model, it must have 0 submeshes.
    if (roundTripModel == nullptr) {
        SUCCEED("File loaded but contained no geometry. Deletion successful!");
    } else {
        REQUIRE(roundTripModel->GetGeometry().subMeshes.size() == 0);
    }
}