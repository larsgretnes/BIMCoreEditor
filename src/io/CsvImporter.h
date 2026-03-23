// =============================================================================
// BimCore/scene/CsvImporter.h
// =============================================================================
#pragma once
#include <string>
#include <unordered_set>

namespace BimCore {

    class CsvImporter {
    public:
        // Scans a CSV file and extracts anything that looks like an IFC GUID
        static std::unordered_set<std::string> ExtractGuids(const std::string& filepath);
    };

} // namespace BimCore
