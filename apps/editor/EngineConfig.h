// =============================================================================
// BimCore/apps/editor/EngineConfig.h
// =============================================================================
#pragma once
#include <GLFW/glfw3.h>
#include <string>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <iostream>

namespace BimCore {

    struct EngineConfig {
        int WindowWidth = 1280;
        int WindowHeight = 720;
        std::string AutoLoadPath = "";

        float BaseSpeed = 500.0f;
        float SprintMultiplier = 5.0f;
        float MouseSensitivityX = 1.0f;
        float MouseSensitivityY = 1.0f;
        float MaxExplodeFactor = 10.0f;

        int KeyToggleNavigation = GLFW_KEY_TAB;
        int KeyToggleLighting   = GLFW_KEY_L;
        int KeyFocus            = GLFW_KEY_F;
        int KeyHide             = GLFW_KEY_H;
        int KeyToggleUI         = GLFW_KEY_F12;
        int KeyMultiSelect      = GLFW_KEY_LEFT_CONTROL;
        int KeyToolSelect       = GLFW_KEY_1;
        int KeyToolPan          = GLFW_KEY_2;
        int KeyToolOrbit        = GLFW_KEY_3;
        int KeyForward          = GLFW_KEY_W;
        int KeyBackward         = GLFW_KEY_S;
        int KeyLeft             = GLFW_KEY_A;
        int KeyRight            = GLFW_KEY_D;
        int KeyUp               = GLFW_KEY_E;
        int KeyDown             = GLFW_KEY_Q;
        int KeySprint           = GLFW_KEY_LEFT_SHIFT;
        int KeyDelete           = GLFW_KEY_DELETE;

        int CadPanButton        = GLFW_MOUSE_BUTTON_MIDDLE;
        int CadOrbitModifier    = GLFW_KEY_LEFT_SHIFT;

        void Load() {
            std::vector<std::string> searchPaths = {
                "settings.ini",
                "build/bin/settings.ini",
                "apps/editor/settings.ini",
                "../apps/editor/settings.ini"
            };

            std::ifstream file;
            for (const auto& path : searchPaths) {
                file.open(path);
                if (file.is_open()) {
                    std::cout << "[Config] Successfully loaded config from: " << path << "\n";
                    break;
                }
            }

            if (!file.is_open()) {
                std::cerr << "[Config] Could not find settings.ini. Using defaults.\n";
                return;
            }

            auto trim = [](const std::string& str) {
                size_t first = str.find_first_not_of(" \t\r\n");
                if (first == std::string::npos) return std::string("");
                size_t last = str.find_last_not_of(" \t\r\n");
                return str.substr(first, last - first + 1);
            };

            std::string line;
            while (std::getline(file, line)) {
                line = trim(line);
                if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[') continue;

                size_t eqPos = line.find('=');
                if (eqPos == std::string::npos) continue;

                ApplySetting(trim(line.substr(0, eqPos)), trim(line.substr(eqPos + 1)));
            }
        }

    private:
        void ApplySetting(const std::string& key, const std::string& val) {
            if (key == "WindowWidth") WindowWidth = std::stoi(val);
            else if (key == "WindowHeight") WindowHeight = std::stoi(val);
            else if (key == "AutoLoadPath") AutoLoadPath = val;
            else if (key == "BaseSpeed") BaseSpeed = std::stof(val);
            else if (key == "SprintMultiplier") SprintMultiplier = std::stof(val);
            else if (key == "MouseSensitivityX") MouseSensitivityX = std::stof(val);
            else if (key == "MouseSensitivityY") MouseSensitivityY = std::stof(val);
            else if (key == "MaxExplodeFactor") MaxExplodeFactor = std::stof(val);
            else {
                int mappedKey = ParseKey(val);
                if (mappedKey != -1) {
                    if (key == "KeyToggleNavigation") KeyToggleNavigation = mappedKey;
                    else if (key == "KeyToggleLighting") KeyToggleLighting = mappedKey;
                    else if (key == "KeyFocus") KeyFocus = mappedKey;
                    else if (key == "KeyHide") KeyHide = mappedKey;
                    else if (key == "KeyToggleUI") KeyToggleUI = mappedKey;
                    else if (key == "KeyMultiSelect") KeyMultiSelect = mappedKey;
                    else if (key == "KeyToolSelect") KeyToolSelect = mappedKey;
                    else if (key == "KeyToolPan") KeyToolPan = mappedKey;
                    else if (key == "KeyToolOrbit") KeyToolOrbit = mappedKey;
                    else if (key == "KeyForward") KeyForward = mappedKey;
                    else if (key == "KeyBackward") KeyBackward = mappedKey;
                    else if (key == "KeyLeft") KeyLeft = mappedKey;
                    else if (key == "KeyRight") KeyRight = mappedKey;
                    else if (key == "KeyUp") KeyUp = mappedKey;
                    else if (key == "KeyDown") KeyDown = mappedKey;
                    else if (key == "KeySprint") KeySprint = mappedKey;
                    else if (key == "KeyDelete") KeyDelete = mappedKey;
                    else if (key == "CadPanButton") CadPanButton = mappedKey;
                    else if (key == "CadOrbitModifier") CadOrbitModifier = mappedKey;
                }
            }
        }

        int ParseKey(const std::string& val) {
            static std::unordered_map<std::string, int> keyMap = {
                {"GLFW_KEY_TAB", GLFW_KEY_TAB}, {"GLFW_KEY_L", GLFW_KEY_L}, {"GLFW_KEY_F", GLFW_KEY_F},
                {"GLFW_KEY_H", GLFW_KEY_H}, {"GLFW_KEY_W", GLFW_KEY_W}, {"GLFW_KEY_A", GLFW_KEY_A},
                {"GLFW_KEY_S", GLFW_KEY_S}, {"GLFW_KEY_D", GLFW_KEY_D}, {"GLFW_KEY_Q", GLFW_KEY_Q},
                {"GLFW_KEY_E", GLFW_KEY_E}, {"GLFW_KEY_1", GLFW_KEY_1}, {"GLFW_KEY_2", GLFW_KEY_2},
                {"GLFW_KEY_3", GLFW_KEY_3}, {"GLFW_KEY_F12", GLFW_KEY_F12}, {"GLFW_KEY_DELETE", GLFW_KEY_DELETE},
                {"GLFW_KEY_LEFT_CONTROL", GLFW_KEY_LEFT_CONTROL}, {"GLFW_KEY_LEFT_SHIFT", GLFW_KEY_LEFT_SHIFT},
                {"GLFW_MOUSE_BUTTON_MIDDLE", GLFW_MOUSE_BUTTON_MIDDLE}, {"GLFW_MOUSE_BUTTON_LEFT", GLFW_MOUSE_BUTTON_LEFT},
                {"GLFW_MOUSE_BUTTON_RIGHT", GLFW_MOUSE_BUTTON_RIGHT}
            };
            if (keyMap.count(val)) return keyMap[val];
            return -1;
        }
    };

} // namespace BimCore
