// =============================================================================
// BimCore/scene/CsvImporter.cpp
// =============================================================================
#include "CsvImporter.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace BimCore {

    std::unordered_set<std::string> CsvImporter::ExtractGuids(const std::string& filepath) {
        std::unordered_set<std::string> foundGuids;
        std::ifstream file(filepath);

        if (!file.is_open()) return foundGuids;

        std::string line;
        while (std::getline(file, line)) {
            // Normalize delimiters to spaces for easy stringstream extraction
            std::replace(line.begin(), line.end(), ',', ' ');
            std::replace(line.begin(), line.end(), ';', ' ');

            std::stringstream ss(line);
            std::string cell;

            while (ss >> cell) {
                // Strip quotes
                cell.erase(std::remove(cell.begin(), cell.end(), '"'), cell.end());

                // IFC GUIDs are strictly 22 characters Base64 encoded
                if (cell.length() == 22) {
                    foundGuids.insert(cell);
                }
            }
        }

        return foundGuids;
    }

} // namespace BimCore
