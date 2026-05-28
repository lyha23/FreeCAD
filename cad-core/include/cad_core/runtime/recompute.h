#pragma once

#include "cad_core/document/model.h"
#include "cad_core/runtime/diagnostics.h"

#include <nlohmann/json.hpp>

#include <vector>

namespace cad_core::runtime {

nlohmann::json recompute(const document::Document& document,
                         std::vector<Diagnostic> diagnostics);

}  // namespace cad_core::runtime
