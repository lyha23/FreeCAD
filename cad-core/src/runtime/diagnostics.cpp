#include "cad_core/runtime/diagnostics.h"

#include <utility>

namespace cad_core::runtime {

void addDiagnostic(std::vector<Diagnostic>& diagnostics,
                   std::string severity,
                   std::string code,
                   std::string message,
                   std::string object,
                   std::string property,
                   std::string stage,
                   std::string target,
                   std::string subname)
{
    diagnostics.push_back({std::move(severity),
                           std::move(code),
                           std::move(message),
                           std::move(object),
                           std::move(property),
                           std::move(stage),
                           std::move(target),
                           std::move(subname)});
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
        if (!diagnostic.stage.empty()) {
            item["stage"] = diagnostic.stage;
        }
        if (!diagnostic.target.empty()) {
            item["target"] = diagnostic.target;
        }
        if (!diagnostic.subname.empty()) {
            item["subname"] = diagnostic.subname;
        }
        result.push_back(item);
    }
    return result;
}

}  // namespace cad_core::runtime
