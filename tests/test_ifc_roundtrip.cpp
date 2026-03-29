// =============================================================================
// BimCore/tests/test_ifc_roundtrip.cpp
// =============================================================================
#include <catch2/catch_test_macros.hpp>
#include "io/IfcLoader.h"
#include "io/IfcExporter.h"
#include "scene/SceneModel.h"
#include <filesystem>

using namespace BimCore;

TEST_CASE("IFC Data Pipeline: Load, Mutate, Save, Verify", "[ifc][io][integration]") {
    std::string sourceFile = "tests/data/wall-extruded-solid.ifc";
    std::string outputFile = "tests/data/temp_wall_output.ifc";

    if (std::filesystem::exists(outputFile)) {
        std::filesystem::remove(outputFile);
    }

    auto model = IfcLoader::LoadDocument(sourceFile);
    REQUIRE(model != nullptr);
    
    uint32_t originalCount = model->GetGeometry().subMeshes.size();
    REQUIRE(originalCount > 0);

    std::string targetGuid = model->GetGeometry().subMeshes[0].guid;

    // 1. Force cache load
    auto props = model->GetElementProperties(targetGuid); 
    
    // 2. Verify engine successfully injects new properties into memory
    bool updated = model->UpdateElementProperty(targetGuid, "AutomatedTestProperty", "SUCCESS");
    REQUIRE(updated == true);
    
    // 3. Verify it flagged it as modified
    REQUIRE(model->HasModifiedProperties(targetGuid) == true);

    // 4. Verify the commit mechanism executes without crashing
    bool committed = model->CommitASTChanges();
    REQUIRE(committed == true);

    // 5. Verify it clears the modification flags after commit!
    REQUIRE(model->HasModifiedProperties(targetGuid) == false);

    // 6. Prove the exporter can successfully write the active tree to disk
    bool exported = IfcExporter::ExportIFC(model, outputFile);
    REQUIRE(exported == true);
    REQUIRE(std::filesystem::exists(outputFile) == true);
}