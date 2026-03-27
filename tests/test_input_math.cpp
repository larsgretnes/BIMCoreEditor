// =============================================================================
// BimCore/tests/test_input_math.cpp
// =============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "input/InputController.h"
#include "ui/UIState.h"

using namespace BimCore;

TEST_CASE("InputController 3D Math", "[input][math]") {
    SelectionState state;
    InputController controller; // <-- THE FIX: Instantiate the controller!
    
    // Set up a fake 3D scene bounding box
    state.sceneMinBounds[0] = -10.0f; state.sceneMaxBounds[0] = 10.0f;
    state.sceneMinBounds[1] = -10.0f; state.sceneMaxBounds[1] = 10.0f;
    state.sceneMinBounds[2] = -10.0f; state.sceneMaxBounds[2] = 10.0f;

    SECTION("CheckPlaneHits ignores planes when showClips is false") {
        state.showPlaneXMax = true;
        state.clipXMax = 5.0f;
        
        // Ray pointing directly at the XMax plane
        Ray ray{{0, 0, 0}, {1, 0, 0}};
        glm::vec3 hitPoint;

        // Called on the object, not the class namespace
        DraggedPlane hit = controller.CheckPlaneHits(ray, state, false, hitPoint);
        REQUIRE(hit == DraggedPlane::None);
    }

    SECTION("CheckPlaneHits correctly calculates intersection on XMax plane") {
        state.showPlaneXMax = true;
        state.clipXMax = 5.0f;
        
        // Shoot a ray from the origin straight down the X axis
        Ray ray{{0, 0, 0}, {1, 0, 0}};
        glm::vec3 hitPoint;

        DraggedPlane hit = controller.CheckPlaneHits(ray, state, true, hitPoint);
        
        REQUIRE(hit == DraggedPlane::XMax);
        // It should hit exactly at X = 5
        REQUIRE_THAT(hitPoint.x, Catch::Matchers::WithinAbs(5.0f, 0.001f));
        REQUIRE_THAT(hitPoint.y, Catch::Matchers::WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(hitPoint.z, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    }

    SECTION("CheckPlaneHits misses if ray points away from the plane") {
        state.showPlaneYMax = true;
        state.clipYMax = 5.0f;
        
        // Shoot a ray backwards (away from Y = 5)
        Ray ray{{0, 0, 0}, {0, -1, 0}};
        glm::vec3 hitPoint;

        DraggedPlane hit = controller.CheckPlaneHits(ray, state, true, hitPoint);
        REQUIRE(hit == DraggedPlane::None);
    }
}