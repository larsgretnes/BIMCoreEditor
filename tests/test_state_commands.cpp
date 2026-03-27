// =============================================================================
// BimCore/tests/test_state_commands.cpp
// =============================================================================
#include <catch2/catch_test_macros.hpp>
#include "core/CommandHistory.h"
#include "ui/UIState.h"

using namespace BimCore;

TEST_CASE("Visibility Commands (Hide/Delete)", "[commands][state]") {
    SelectionState state;
    CommandHistory history;

    // Reset state before tests just to be absolutely certain
    state.hiddenObjects.clear();
    state.deletedObjects.clear();
    state.hiddenStateChanged = false;

    SECTION("CmdHide toggles hidden status and supports Undo/Redo") {
        std::vector<std::string> target = {"WALL-001", "DOOR-002"};
        
        REQUIRE(state.hiddenObjects.count("WALL-001") == 0);

        // 1. Hide the elements
        history.ExecuteCommand(std::make_unique<CmdHide>(state, target, true));
        
        REQUIRE(state.hiddenObjects.count("WALL-001") == 1);
        REQUIRE(state.hiddenObjects.count("DOOR-002") == 1);
        REQUIRE(state.hiddenStateChanged == true); // UI needs to know to redraw!

        // 2. Undo the hide
        history.Undo();
        REQUIRE(state.hiddenObjects.count("WALL-001") == 0);
        REQUIRE(state.hiddenObjects.count("DOOR-002") == 0);

        // 3. Redo the hide
        history.Redo();
        REQUIRE(state.hiddenObjects.count("WALL-001") == 1);
    }

    SECTION("CmdDelete marks objects as both deleted AND hidden") {
        std::vector<std::string> target = {"WINDOW-999"};

        // 1. Delete the element
        history.ExecuteCommand(std::make_unique<CmdDelete>(state, target));
        
        // Deleting should hide it from the viewport AND add it to the deleted pool
        REQUIRE(state.deletedObjects.count("WINDOW-999") == 1);
        REQUIRE(state.hiddenObjects.count("WINDOW-999") == 1);
        REQUIRE(state.hiddenStateChanged == true);

        // 2. Undo the deletion
        history.Undo();
        REQUIRE(state.deletedObjects.count("WINDOW-999") == 0);
        REQUIRE(state.hiddenObjects.count("WINDOW-999") == 0);
    }
}