// =============================================================================
// BimCore/scene/FormatModules.h
// =============================================================================
#pragma once
#include <string>
#include <memory>
#include "BimDocument.h"

namespace BimCore {

    class BcfImporter {
    public:
        static void Import(const std::string& filepath, std::shared_ptr<BimDocument> document);
    };

    class GltfImporter {
    public:
        static void Import(const std::string& filepath, std::shared_ptr<BimDocument> document);
    };

    class GltfExporter {
    public:
        static void Export(const std::string& filepath, std::shared_ptr<BimDocument> document);
    };

} // namespace BimCore
