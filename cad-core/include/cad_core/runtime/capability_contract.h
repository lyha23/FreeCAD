#pragma once

#include <nlohmann/json.hpp>

namespace cad_core::runtime
{

nlohmann::json cadCoreVersionJson();
nlohmann::json capabilityContractJson();

}  // namespace cad_core::runtime
