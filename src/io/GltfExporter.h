// =============================================================================
// BimCore/io/GltfExporter.h
// =============================================================================
#pragma once
#include <string>
#include <memory>
#include "scene/SceneModel.h"

namespace BimCore {
    class GltfExporter {
    public:
        static void Export(const std::string& filepath, std::shared_ptr<SceneModel> document);
    };
} // namespace BimCore