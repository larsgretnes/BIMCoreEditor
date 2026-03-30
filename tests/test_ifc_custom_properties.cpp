// =============================================================================
// BimCore/tests/test_ifc_custom_properties.cpp
// =============================================================================
#include <catch2/catch_test_macros.hpp>
#include "io/IfcLoader.h"
#include "io/IfcExporter.h"
#include "scene/SceneModel.h"
#include <filesystem>

using namespace BimCore;

TEST_CASE("IFC Data Pipeline: Inject Custom Psets", "[ifc][io][integration][pset]") {
    std::string sourceFile = "tests/data/wall-extruded-solid.ifc";
    std::string outputFile = "tests/data/temp_custom_pset.ifc";

    if (std::filesystem::exists(outputFile)) {
        std::filesystem::remove(outputFile);
    }

    auto model = IfcLoader::LoadDocument(sourceFile);
    REQUIRE(model != nullptr);
    REQUIRE(model->GetGeometry().subMeshes.size() > 0);

    std::string targetGuid = model->GetGeometry().subMeshes[0].guid;

    // 1. Inject a brand new Custom Property into the AST
    bool added = model->AddCustomProperty(targetGuid, "BIMCore_CustomData", "Manufacturer", "AcmeCorp");
    REQUIRE(added == true);

    // 2. Verify it immediately appeared in the engine's memory cache
    auto props = model->GetElementProperties(targetGuid);
    REQUIRE(props.count("Manufacturer") == 1);
    REQUIRE(props["Manufacturer"].value == "AcmeCorp");

    // 3. Export the file. Because we injected raw entities into the AST, 
    // the exporter should flawlessly serialize them without needing CommitASTChanges!
    bool exported = IfcExporter::ExportIFC(model, outputFile);
    REQUIRE(exported == true);

    // 4. Round-Trip Verification
    auto roundTripModel = IfcLoader::LoadDocument(outputFile);
    REQUIRE(roundTripModel != nullptr);

    auto roundTripProps = roundTripModel->GetElementProperties(targetGuid);
    REQUIRE(roundTripProps.count("Manufacturer") == 1);
    REQUIRE(roundTripProps["Manufacturer"].value == "AcmeCorp");
}