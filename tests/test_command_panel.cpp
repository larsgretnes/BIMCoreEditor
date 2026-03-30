// =============================================================================
// tests/test_command_panel.cpp
// =============================================================================
#include <catch2/catch_test_macros.hpp>
#include "ui/UICommandPanel.h"

using namespace BimCore;

TEST_CASE("Command Panel Tokenizer correctly splits inputs", "[CommandTerminal]") {
    
    SECTION("Single command") {
        auto tokens = UICommandPanel::TokenizeCommand("clear");
        REQUIRE(tokens.size() == 1);
        REQUIRE(tokens[0] == "clear");
    }

    SECTION("Command with one numeric argument") {
        auto tokens = UICommandPanel::TokenizeCommand("set_speed 10.5");
        REQUIRE(tokens.size() == 2);
        REQUIRE(tokens[0] == "set_speed");
        REQUIRE(tokens[1] == "10.5");
    }

    SECTION("Command with excessive whitespace") {
        auto tokens = UICommandPanel::TokenizeCommand("   explode     5.0   ");
        REQUIRE(tokens.size() == 2);
        REQUIRE(tokens[0] == "explode");
        REQUIRE(tokens[1] == "5.0");
    }
}