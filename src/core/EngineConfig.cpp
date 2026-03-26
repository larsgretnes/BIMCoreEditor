// =============================================================================
// BimCore/apps/editor/EngineConfig.cpp
// =============================================================================
#include "core/EngineConfig.h"
#include <mini/ini.h>  
#include <iostream>

namespace BimCore {
    void EngineConfig::Load() {
        mINI::INIFile file("settings.ini");
        mINI::INIStructure ini;

        if (file.read(ini)) {
            if (ini.has("App") && ini["App"].has("AutoLoadPath")) {
                AutoLoadPath = ini["App"]["AutoLoadPath"];
            }

            if (ini.has("Controls")) {
                if (ini["Controls"].has("CadPanSpeed")) CadPanSpeed = std::stof(ini["Controls"]["CadPanSpeed"]);
                if (ini["Controls"].has("CadOrbitSpeed")) CadOrbitSpeed = std::stof(ini["Controls"]["CadOrbitSpeed"]);
                if (ini["Controls"].has("KeyboardOrbitSpeed")) KeyboardOrbitSpeed = std::stof(ini["Controls"]["KeyboardOrbitSpeed"]);
                if (ini["Controls"].has("FlightMouseSpeed")) FlightMouseSpeed = std::stof(ini["Controls"]["FlightMouseSpeed"]);
                if (ini["Controls"].has("ZoomSpeed")) ZoomSpeed = std::stof(ini["Controls"]["ZoomSpeed"]);
                if (ini["Controls"].has("KeyToolMeasure")) KeyToolMeasure = std::stoi(ini["Controls"]["KeyToolMeasure"]);

                if (ini["Controls"].has("ZoomFlyMultiplier")) ZoomFlyMultiplier = std::stof(ini["Controls"]["ZoomFlyMultiplier"]);
                if (ini["Controls"].has("CameraFocusSpeed")) CameraFocusSpeed = std::stof(ini["Controls"]["CameraFocusSpeed"]);
                if (ini["Controls"].has("CameraFocusPadding")) CameraFocusPadding = std::stof(ini["Controls"]["CameraFocusPadding"]);
                if (ini["Controls"].has("CameraMinOrbitDistance")) CameraMinOrbitDistance = std::stof(ini["Controls"]["CameraMinOrbitDistance"]);
                if (ini["Controls"].has("CameraPivotJumpThreshold")) CameraPivotJumpThreshold = std::stof(ini["Controls"]["CameraPivotJumpThreshold"]);
                if (ini["Controls"].has("CameraPanReferenceHeight")) CameraPanReferenceHeight = std::stof(ini["Controls"]["CameraPanReferenceHeight"]);
            }

            if (ini.has("Theme")) {
                auto parseColor = [](const std::string& str, float* out) {
                    if (str.empty()) return;
                    float r, g, b, a;
                    if (sscanf(str.c_str(), "%f,%f,%f,%f", &r, &g, &b, &a) == 4) {
                        out[0] = r; out[1] = g; out[2] = b; out[3] = a;
                    }
                };
                parseColor(ini["Theme"]["Background"], ThemeBg);
                parseColor(ini["Theme"]["Panel"], ThemePanel);
                parseColor(ini["Theme"]["Accent"], ThemeAccent);
                parseColor(ini["Theme"]["Text"], ThemeText);
            }
        } else {
            std::cout << "[Config] settings.ini not found, writing default configuration...\n";

            ini["App"]["AutoLoadPath"] = "../test.ifc";
            AutoLoadPath = "../test.ifc";

            ini["Controls"]["CadPanSpeed"] = std::to_string(CadPanSpeed);
            ini["Controls"]["CadOrbitSpeed"] = std::to_string(CadOrbitSpeed);
            ini["Controls"]["KeyboardOrbitSpeed"] = std::to_string(KeyboardOrbitSpeed);
            ini["Controls"]["FlightMouseSpeed"] = std::to_string(FlightMouseSpeed);
            ini["Controls"]["ZoomSpeed"] = std::to_string(ZoomSpeed);
            ini["Controls"]["KeyToolMeasure"] = std::to_string(KeyToolMeasure);

            ini["Controls"]["ZoomFlyMultiplier"] = std::to_string(ZoomFlyMultiplier);
            ini["Controls"]["CameraFocusSpeed"] = std::to_string(CameraFocusSpeed);
            ini["Controls"]["CameraFocusPadding"] = std::to_string(CameraFocusPadding);
            ini["Controls"]["CameraMinOrbitDistance"] = std::to_string(CameraMinOrbitDistance);
            ini["Controls"]["CameraPivotJumpThreshold"] = std::to_string(CameraPivotJumpThreshold);
            ini["Controls"]["CameraPanReferenceHeight"] = std::to_string(CameraPanReferenceHeight);

            ini["Theme"]["Background"] = "0.11,0.11,0.13,1.0";
            ini["Theme"]["Panel"]      = "0.16,0.16,0.19,1.0";
            ini["Theme"]["Accent"]     = "0.94,0.65,0.00,1.0";
            ini["Theme"]["Text"]       = "0.88,0.88,0.88,1.0";

            file.write(ini);
        }
    }
}