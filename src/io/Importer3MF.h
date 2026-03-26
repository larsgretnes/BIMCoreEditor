// =============================================================================
// BimCore/io/Importer3MF.h
// =============================================================================
#pragma once

#include <string>
#include <memory>
#include "scene/SceneModel.h"

namespace BimCore {

    class Importer3MF {
    public:
        // Leser en 3MF-fil (ZIP) og injiserer geometrien i targetModel
        static bool Import(const std::string& filepath, std::shared_ptr<SceneModel> targetModel);
    };

} // namespace BimCore