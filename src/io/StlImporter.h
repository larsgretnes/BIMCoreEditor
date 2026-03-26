// =============================================================================
// BimCore/io/StlImporter.h
// =============================================================================
#pragma once

#include <string>
#include <memory>
#include "scene/SceneModel.h"

namespace BimCore {

    class StlImporter {
    public:
        // Reads an STL file and injects it into the target SceneModel
        static bool Import(const std::string& filepath, std::shared_ptr<SceneModel> targetModel);
    };

} // namespace BimCore