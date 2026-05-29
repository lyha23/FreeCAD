#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace cad_core::runtime {

struct Diagnostic {
    std::string severity;
    std::string code;
    std::string message;
    std::string object;
    std::string property;
    std::string stage;
    std::string target;
    std::string subname;
};

void addDiagnostic(std::vector<Diagnostic>& diagnostics,
                   std::string severity,
                   std::string code,
                   std::string message,
                   std::string object = {},
                   std::string property = {},
                   std::string stage = {},
                   std::string target = {},
                   std::string subname = {});

nlohmann::json diagnosticsToJson(const std::vector<Diagnostic>& diagnostics);

}  // namespace cad_core::runtime
