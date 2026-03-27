// =============================================================================
// BimCore/tests/test_property_state.cpp
// =============================================================================
#include <catch2/catch_test_macros.hpp>
#include "ui/UIState.h"
#include <string>

using namespace BimCore;

TEST_CASE("SelectionState Property Tracking", "[state][properties]") {
    SelectionState state;
    std::string mockGuid = "IFC-WALL-12345";

    // Reset state
    state.originalProperties.clear();
    state.deletedProperties.clear();

    SECTION("Editing a property backs up the original value") {
        // Simulate the user editing "FireRating" from "60" to "120"
        std::string propKey = "FireRating";
        std::string originalVal = "60";
        std::string newVal = "120";

        // The UI should store the original before applying the edit
        if (state.originalProperties[mockGuid].find(propKey) == state.originalProperties[mockGuid].end()) {
            state.originalProperties[mockGuid][propKey] = originalVal;
        }

        // Assert the backup was created correctly
        REQUIRE(state.originalProperties.count(mockGuid) == 1);
        REQUIRE(state.originalProperties[mockGuid][propKey] == "60");
        
        // If the user edits it AGAIN (from 120 to 90), the original "60" should NOT be overwritten
        if (state.originalProperties[mockGuid].find(propKey) == state.originalProperties[mockGuid].end()) {
            state.originalProperties[mockGuid][propKey] = newVal; // This should be skipped!
        }

        REQUIRE(state.originalProperties[mockGuid][propKey] == "60");
    }

    SECTION("Deleting a property registers in the deleted pool") {
        std::string propKey = "AcousticRating";
        
        state.deletedProperties[mockGuid].insert(propKey);

        REQUIRE(state.deletedProperties.count(mockGuid) == 1);
        REQUIRE(state.deletedProperties[mockGuid].count(propKey) == 1);
        
        // Simulating an Undo action
        state.deletedProperties[mockGuid].erase(propKey);
        REQUIRE(state.deletedProperties[mockGuid].count(propKey) == 0);
    }
}