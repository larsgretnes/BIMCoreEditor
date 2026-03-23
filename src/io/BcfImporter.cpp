// =============================================================================
// BimCore/io/BcfImporter.cpp
// =============================================================================
#include "BcfImporter.h"
#include <iostream>

namespace BimCore {
    void BcfImporter::Import(const std::string& filepath, std::shared_ptr<SceneModel> document) {
        std::cout << "[BIMCore] Stub: Preparing to parse BCF XML from " << filepath << "\n";
    }
} // namespace BimCore