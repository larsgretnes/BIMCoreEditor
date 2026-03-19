#pragma once
#include <string>
#include <memory>
#include "scene/BimDocument.h"
#include "scene/IfcLoader.h" // For the LoadState tracker

namespace BimCore {
    class IfcExporter {
    public:
        // Notice we are passing the thread-safe std::shared_ptr now!
        static bool ExportIFC(std::shared_ptr<BimDocument> document, const std::string& destinationFile, LoadState* state = nullptr);
    };
}
