#pragma once

#include "cad_core/app/property.h"
#include "cad_core/runtime/diagnostics.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace cad_core::app {

PropertyValue parsePropertyValue(const std::string& objectName,
                                 const std::string& propertyName,
                                 const nlohmann::json& raw,
                                 std::vector<runtime::Diagnostic>& diagnostics);

}  // namespace cad_core::app
