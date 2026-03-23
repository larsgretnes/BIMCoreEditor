#pragma once
#include <string>
#include <memory>
#include "scene/SceneModel.h"
#include "io/IfcLoader.h" // For the LoadState tracker

namespace BimCore {
    class IfcExporter {
    public:
        // Notice we are passing the thread-safe std::shared_ptr now!
        static bool ExportIFC(std::shared_ptr<SceneModel> document, const std::string& destinationFile, LoadState* state = nullptr);
    };
}
