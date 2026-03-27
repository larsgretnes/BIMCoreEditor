// =============================================================================
// BimCore/tests/test_command_history.cpp
// =============================================================================
#include <catch2/catch_test_macros.hpp>
#include "core/CommandHistory.h"

using namespace BimCore;

// A simple isolated command to test the raw history stack logic 
// without dragging in the 3D graphics or IFC libraries.
class TestMathCommand : public ICommand {
public:
    int& target;
    int oldValue;
    int newValue;

    TestMathCommand(int& t, int val) : target(t), oldValue(t), newValue(val) {}
    
    void Execute() override { target = newValue; }
    void Undo() override { target = oldValue; }
    std::string GetName() const override { return "Math Test"; }
};

TEST_CASE("CommandHistory Undo and Redo Mechanics", "[history]") {
    CommandHistory history;
    int testValue = 10;

    SECTION("Executing a command updates the value and stack") {
        history.ExecuteCommand(std::make_unique<TestMathCommand>(testValue, 20));
        
        REQUIRE(testValue == 20);
        REQUIRE(history.CanUndo() == true);
        REQUIRE(history.CanRedo() == false);
        REQUIRE(history.GetLastCommandName() == "Math Test");
    }

    SECTION("Undoing a command reverts the value") {
        history.ExecuteCommand(std::make_unique<TestMathCommand>(testValue, 20));
        history.Undo();
        
        REQUIRE(testValue == 10);
        REQUIRE(history.CanUndo() == false);
        REQUIRE(history.CanRedo() == true);
    }

    SECTION("Redoing a command re-applies the value") {
        history.ExecuteCommand(std::make_unique<TestMathCommand>(testValue, 20));
        history.Undo();
        history.Redo();
        
        REQUIRE(testValue == 20);
        REQUIRE(history.CanUndo() == true);
        REQUIRE(history.CanRedo() == false);
    }

    SECTION("Executing a new command clears the redo stack (branching timeline)") {
        history.ExecuteCommand(std::make_unique<TestMathCommand>(testValue, 20));
        history.Undo();
        
        // We are back at 10. Now we branch the timeline by executing a new command.
        history.ExecuteCommand(std::make_unique<TestMathCommand>(testValue, 99));
        
        REQUIRE(testValue == 99);
        REQUIRE(history.CanUndo() == true);
        
        // The path to '20' is destroyed because we altered the timeline
        REQUIRE(history.CanRedo() == false); 
    }
}