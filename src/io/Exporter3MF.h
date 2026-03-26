// =============================================================================
// BimCore/io/Exporter3MF.h
// =============================================================================
#pragma once

#include <string>
#include <memory>
#include "scene/SceneModel.h"

namespace BimCore {

    class Exporter3MF {
    public:
        // Tar all geometri fra SceneModel og eksporterer den som en 3MF-arkivfil
        static bool Export(const std::string& filepath, std::shared_ptr<SceneModel> sourceModel);
    };

} // namespace BimCore