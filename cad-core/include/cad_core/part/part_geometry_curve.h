#pragma once

#include "cad_core/runtime/compute_context.h"

#include <nlohmann/json.hpp>

namespace cad_core::part
{

bool isPartGeometryCurveRequest(const nlohmann::json& raw);
runtime::ComputeContext computePartGeometryCurveRequest(const nlohmann::json& raw);
nlohmann::json partGeometryCurveResultJson(const runtime::ComputeContext& context);
nlohmann::json partGeometryCurveLegacyResultJson(const runtime::ComputeContext& context);

}  // namespace cad_core::part
