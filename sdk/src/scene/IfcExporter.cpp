#include "Core.h"
#include "IfcExporter.h"
#include <fstream>
#include <iostream>

namespace BimCore {

    bool IfcExporter::ExportIFC(std::shared_ptr<BimDocument> document, const std::string& destinationFile, LoadState* state) {
        if (state) state->SetStatus("Validating AST Data...", 0.1f);

        if (!document || !document->GetDatabase()) {
            if (state) state->SetStatus("Error: No document loaded to export.", 1.0f);
            return false;
        }

        if (state) state->SetStatus("Serializing AST to disk...", 0.5f);

        try {
            std::ofstream outFile(destinationFile);
            if (!outFile.good()) {
                if (state) state->SetStatus("Error: Cannot write to destination path.", 1.0f);
                return false;
            }

            // The beauty of the OpenBIM standard:
            // IfcOpenShell natively overloads the C++ stream operator to write the perfectly compliant STEP graph!
            outFile << *document->GetDatabase();
            outFile.close();

            if (state) state->SetStatus("Export Complete!", 1.0f);
            return true;

        } catch (...) {
            if (state) state->SetStatus("Fatal Error during AST Serialization.", 1.0f);
            return false;
        }
    }

} // namespace BimCore
