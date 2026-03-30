// =============================================================================
// BimCore/core/EngineConfig.h
// =============================================================================
#pragma once
#include <string>

namespace BimCore {
    struct EngineConfig {
        // --- Centralized Application Info ---
        std::string AppName = "BIMCore Editor";
        std::string AppVersion = "0.5"; // <--- MILEPÆL: Oppdatert til 0.5!

        int WindowWidth = 1600;  
        int WindowHeight = 900;  

        float BaseSpeed = 5.0f;
        float FlightFastMultiplier = 3.0f; 
        float FlightSlowMultiplier = 0.2f; 
        float MouseSensitivityX = 0.005f;
        float MouseSensitivityY = 0.005f;

        float ZoomSpeed = 1.0f;
        float ZoomFastMultiplier = 3.0f;   
        float ZoomSlowMultiplier = 0.2f;

        float ZoomFlyMultiplier = 1.0f;     
        float CameraFocusSpeed = 2.5f;      
        float CameraFocusPadding = 1.2f;    
        float CameraMinOrbitDistance = 0.1f; 
        float CameraPivotJumpThreshold = 0.5f; 
        float CameraPanReferenceHeight = 1080.0f; 

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
        int KeyFast = 340; 
        int KeySlow = 342; 
        int KeyMultiSelect = 341;
        int KeyDelete = 261;
        int KeyHide = 72;
        int KeyFocus = 70;
        int KeyToggleUI = 85;
        int KeyToggleCommandPanel = 96; 
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