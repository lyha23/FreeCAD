#pragma once

#include "cad_core/app/document.h"
#include "cad_core/runtime/compute_context.h"
#include "cad_core/runtime/diagnostics.h"

#include <nlohmann/json.hpp>

#include <vector>

namespace cad_core::runtime {

ComputeContext recomputeContext(const app::Document& document,
                                std::vector<Diagnostic> diagnostics);
nlohmann::json recomputeResultJson(const app::Document& document,
                                   const ComputeContext& context);
nlohmann::json recompute(const app::Document& document,
                         std::vector<Diagnostic> diagnostics);

}  // namespace cad_core::runtime
