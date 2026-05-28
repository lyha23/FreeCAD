#include "cad_core/runtime/diagnostics.h"

#include <utility>

namespace cad_core::runtime {

void addDiagnostic(std::vector<Diagnostic>& diagnostics,
                   std::string severity,
                   std::string code,
                   std::string message,
                   std::string object,
                   std::string property)
{
    diagnostics.push_back({std::move(severity),
                           std::move(code),
                           std::move(message),
                           std::move(object),
                           std::move(property)});
}

nlohmann::json diagnosticsToJson(const std::vector<Diagnostic>& diagnostics)
{
    nlohmann::json result = nlohmann::json::array();
    for (const auto& diagnostic : diagnostics) {
        nlohmann::json item = {
            {"severity", diagnostic.severity},
            {"code", diagnostic.code},
            {"message", diagnostic.message},
        };
        if (!diagnostic.object.empty()) {
            item["object"] = diagnostic.object;
        }
        if (!diagnostic.property.empty()) {
            item["property"] = diagnostic.property;
        }
        result.push_back(item);
    }
    return result;
}

}  // namespace cad_core::runtime

