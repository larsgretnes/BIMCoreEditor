// =============================================================================
// BIMCore Editor — main.cpp
// =============================================================================
#include "EditorApp.h"
#include <iostream>

int main() {
    try {
        BimCore::EditorApp app;

        if (app.Initialize()) {
            app.Run();
        } else {
            std::cerr << "[BIMCore] Failed to initialize EditorApp.\n";
            return -1;
        }
    } catch (const std::exception& e) {
        std::cerr << "[BIMCore] FATAL ERROR: " << e.what() << "\n";
        return -1;
    }

    return 0;
}
