// =============================================================================
// BimCore/io/StlExporter.h
// =============================================================================
#pragma once

#include <string>
#include <memory>
#include "scene/SceneModel.h"

namespace BimCore {

    class StlExporter {
    public:
        // Takes the current SceneModel geometry and dumps it to a Binary STL
        static bool Export(const std::string& filepath, std::shared_ptr<SceneModel> sourceModel);
    };

} // namespace BimCore