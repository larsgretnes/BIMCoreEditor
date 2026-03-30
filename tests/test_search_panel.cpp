// =============================================================================
// tests/test_search_panel.cpp
// =============================================================================
#include <catch2/catch_test_macros.hpp>
#include "ui/UISearchPanel.h"

using namespace BimCore;

TEST_CASE("Search Parser handles standard and complex inputs", "[SearchEngine]") {
    std::string key, value;

    SECTION("Global blunt search (no colons)") {
        UISearchPanel::ParseSearchQuery("Office Chair", key, value);
        REQUIRE(key == "");
        REQUIRE(value == "office chair");
    }

    SECTION("Standard Key-Value search") {
        UISearchPanel::ParseSearchQuery("Name:Chair", key, value);
        REQUIRE(key == "name");
        REQUIRE(value == "chair");
    }

    SECTION("Wildcard stripping (*) - Implicit contains") {
        UISearchPanel::ParseSearchQuery("*name*:*chair*", key, value);
        REQUIRE(key == "name");
        REQUIRE(value == "chair");
    }

    SECTION("Handling colons inside the value using quotes") {
        // Bruker søker på egenskapen 'Description', og verdien 'Type: A'
        UISearchPanel::ParseSearchQuery("Description:\"Type: A\"", key, value);
        REQUIRE(key == "description");
        REQUIRE(value == "type: a");
    }

    SECTION("Handling colons inside the key using quotes") {
        // Bruker søker på en egenskap som heter 'Pset_Wall:Common', og verdien 'Brick'
        UISearchPanel::ParseSearchQuery("\"Pset_Wall:Common\":Brick", key, value);
        REQUIRE(key == "pset_wall:common");
        REQUIRE(value == "brick");
    }

    SECTION("Whitespace trimming") {
        UISearchPanel::ParseSearchQuery("   Type   :   Wall   ", key, value);
        REQUIRE(key == "type");
        REQUIRE(value == "wall");
    }
}