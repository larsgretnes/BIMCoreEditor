// =============================================================================
// BimCore/io/BcfImporter.h
// =============================================================================
#pragma once
#include <string>
#include <memory>
#include "scene/SceneModel.h"

namespace BimCore {
    class BcfImporter {
    public:
        static void Import(const std::string& filepath, std::shared_ptr<SceneModel> document);
    };
} // namespace BimCore