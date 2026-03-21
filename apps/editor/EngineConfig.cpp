// =============================================================================
// BimCore/EngineConfig.cpp
// =============================================================================
#include "EngineConfig.h"
#include "platform/ini.h"
#include <iostream>

namespace BimCore {
    void EngineConfig::Load() {
        mINI::INIFile file("settings.ini");
        mINI::INIStructure ini;

        if (file.read(ini)) {
            if (ini.has("App") && ini["App"].has("AutoLoadPath")) {
                AutoLoadPath = ini["App"]["AutoLoadPath"];
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

            // --- FIXED: Default path pointing to the root folder ---
            ini["App"]["AutoLoadPath"] = "../../test.ifc";
            AutoLoadPath = "../../test.ifc";

            ini["Theme"]["Background"] = "0.11,0.11,0.13,1.0";
            ini["Theme"]["Panel"]      = "0.16,0.16,0.19,1.0";
            ini["Theme"]["Accent"]     = "0.94,0.65,0.00,1.0";
            ini["Theme"]["Text"]       = "0.88,0.88,0.88,1.0";

            file.write(ini);
        }
    }
}
