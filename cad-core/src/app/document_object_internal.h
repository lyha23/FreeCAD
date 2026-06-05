#pragma once

#include "cad_core/app/document_object.h"
#include "cad_core/runtime/diagnostics.h"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace cad_core::app {

std::optional<DocumentObject> parseDocumentObject(const nlohmann::json& item,
                                                  std::size_t index,
                                                  std::set<std::string>& seenNames,
                                                  std::set<long long>& seenIds,
                                                  std::vector<runtime::Diagnostic>& diagnostics);

}  // namespace cad_core::app
