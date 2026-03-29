// =============================================================================
// BimCore/tests/test_search.cpp
// =============================================================================
#include <catch2/catch_test_macros.hpp>
#include "ui/UISearchPanel.h"
#include "ui/UIState.h"
#include "io/IfcLoader.h"
#include <vector>
#include <memory>

using namespace BimCore;

TEST_CASE("Global Text Search Engine", "[search][integration]") {
    SelectionState state;
    std::vector<std::shared_ptr<SceneModel>> documents;

    // Use the official buildingSMART fixture
    auto doc = IfcLoader::LoadDocument("tests/data/wall-extruded-solid.ifc");
    REQUIRE(doc != nullptr);
    documents.push_back(doc);

    SECTION("Search is case-insensitive and finds types") {
        REQUIRE(doc->GetGeometry().subMeshes.size() > 0);
        std::string actualType = doc->GetGeometry().subMeshes[0].type;

        UISearchPanel::ExecuteTextSearch(actualType, documents, state);
        
        REQUIRE(state.objects.size() > 0);
        REQUIRE(state.selectionChanged == true);
    }

    SECTION("Search returns nothing for garbage queries") {
        UISearchPanel::ExecuteTextSearch("THIS_DOES_NOT_EXIST_12345", documents, state);
        REQUIRE(state.objects.size() == 0);
    }
}