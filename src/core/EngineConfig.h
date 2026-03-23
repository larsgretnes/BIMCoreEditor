// =============================================================================
// BimCore/EngineConfig.h
// =============================================================================
#pragma once
#include <string>

namespace BimCore {
    struct EngineConfig {
        // --- Centralized Application Info ---
        std::string AppName = "BIMCore Editor";
        std::string AppVersion = "0.2";

        int WindowWidth = 1280;
        int WindowHeight = 720;

        float BaseSpeed = 5.0f;
        float SprintMultiplier = 3.0f;
        float MouseSensitivityX = 0.005f;
        float MouseSensitivityY = 0.005f;

        float ZoomSpeed = 1.0f;
        float ZoomSlowMultiplier = 0.2f;

        // --- NEW: Extracted Camera Math Constants ---
        float ZoomFlyMultiplier = 1.0f;     // Doubled speed!
        float CameraFocusSpeed = 2.5f;      // Animation speed for focusing
        float CameraFocusPadding = 1.2f;    // Bounding box padding when focusing
        float CameraMinOrbitDistance = 0.1f; // Pivot push threshold
        float CameraPivotJumpThreshold = 0.5f; // Distance required to animate a pivot change
        float CameraPanReferenceHeight = 1080.0f; // Screen height mapping for pan speed

        float CadPanSpeed        = 250.0f;
        float CadOrbitSpeed      = 250.0f;
        float KeyboardOrbitSpeed = 500.0f;
        float FlightMouseSpeed   = 5.0f;

        float MaxExplodeFactor = 5.0f;
        std::string AutoLoadPath = "";

        float ThemeBg[4]     = {0.11f, 0.11f, 0.13f, 1.0f};
        float ThemePanel[4]  = {0.16f, 0.16f, 0.19f, 1.0f};
        float ThemeAccent[4] = {0.94f, 0.65f, 0.00f, 1.0f};
        float ThemeText[4]   = {0.88f, 0.88f, 0.88f, 1.0f};

        int KeyForward = 87;
        int KeyBackward = 83;
        int KeyLeft = 65;
        int KeyRight = 68;
        int KeyUp = 69;
        int KeyDown = 81;
        int KeySprint = 340;
        int KeyMultiSelect = 341;
        int KeyDelete = 261;
        int KeyHide = 72;
        int KeyFocus = 70;
        int KeyToggleUI = 85;
        int KeyToggleNavigation = 258;
        int KeyToggleLighting = 76;
        int KeyToolSelect = 49;
        int KeyToolPan = 50;
        int KeyToolOrbit = 51;
        int KeyToolMeasure = 77;
        int CadPanButton = 2;
        int CadOrbitModifier = 340;

        void Load();
    };
}